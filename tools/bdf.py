"""
weegfx/tools/bdf.py: BDF font format parsing

Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
Released under the 3-clause BSD license (see LICENSE)
"""

import os
import sys
import re
from typing import TextIO, List, Iterable, Any, Callable

from font import row_width, BBox

def bdf_width(width: int) -> int:
    """Calculates the width in bits of each row in the BDF from the actual witdth of a character in pixels."""
    # NOTE: Lines in BDF BITMAPs are always stored in multiples of 8 bits
    # (https://stackoverflow.com/a/37944252)
    return -((-width) // 8) * 8


def nextline(stream: TextIO) -> str:
    """Returns the next non-whitespace line in `stream` (stripped) or an empty line on EOF."""
    line = stream.readline()
    while line.isspace() or line.startswith('COMMENT'):
        line = stream.readline()
    return line.strip()


class BdfBitmap:
    def __init__(self, width: int, height: int, data: Iterable[int] = []):
        self.data = list(data)
        self.width = int(width)
        self.bdf_width = row_width(self.width)
        self.height = int(height)

    @staticmethod
    def parse_from(stream: TextIO, width: int, height: int, first_line: str = None) -> 'BdfBitmap':
        """Parses a BDF bitmap from the given stream, given its expected width and height(in pixels).
        If `first_line` is present, uses it instead of fetching a first line from the stream.
        """
        first_line = first_line if first_line is not None else nextline(stream)
        if not first_line.startswith('BITMAP'):
            raise SyntaxError(f'Expected BITMAP but found {first_line}')

        bitmap = BdfBitmap(width, height)

        bits_read = 0
        bits_to_read = bitmap.bdf_width * bitmap.height
        while bits_read < bits_to_read:
            line_hex = ''.join(ch for ch in nextline(
                stream) if not ch.isspace())

            for col in range(0, len(line_hex), 2):
                bitmap.data.append(int(line_hex[col:col+2], 16))
                bits_read += 8

        return bitmap

    def __repr__(self) -> str:
        return f'BdfBitmap({self.width}x{self.height})'


class BdfProperty(tuple):
    """The value of a BDF property as a tuple of ints/strings."""

    VALUE_RE = re.compile(
        r'\s*(?:(?P<int>-?\d+)|(?P<quoted>"[^"]*")|(?P<unquoted>.+))')

    def __new__(cls, values_str: str):
        """Inits a BDF property value given the string representing it in the BDF file.
        Value(s) are parsed as either ints or strings."""
        return tuple.__new__(cls, cls._parse_value(values_str))

    @classmethod
    def _parse_value(cls, value_str: str) -> Any:
        values = []
        for match in cls.VALUE_RE.finditer(value_str):
            if not match:
                raise SyntaxError(
                    f'Expected int or string value, got {repr(value_str)}')

            integer, quoted, unquoted = match.groups()
            if integer is not None:
                value = int(integer)
            elif quoted is not None:
                value = quoted[1:-1]
            else:
                value = unquoted
            values.append(value)

        return tuple(values)


class BdfRecord:
    def __init__(self, type: str, args: Iterable):
        self.type = str(type)
        """The type of record(FONT, CHAR...)"""
        self.args = list(args)
        """The arguments present after the START < type > directive"""
        self.items = {}
        """A mapping of `field -> <BdfProperty(field, args...) or BdfBitmap where field = 'BITMAP' > ."""
        self.children = []
        """A list of `BdfRecord`s that are nested into this one."""

    @staticmethod
    def parse_from(stream: TextIO, expected_type: str = None, first_line: str = None) -> 'BdfRecord':
        """Parses a BDF record from the given stream.
        If `expected_type` is present, throws an exception if the record is not of the given type.
        If `first_line` is present, uses it instead of fetching a first line from the stream.
        """
        first_line = first_line if first_line is not None else nextline(stream)

        expected_start_tag = 'START' + expected_type if expected_type else ''
        if not first_line.startswith(expected_start_tag):
            raise SyntaxError(
                f'Expected {expected_start_tag} but found {first_line}')

        this_start_tag, *this_args = first_line.split()
        record = BdfRecord(this_start_tag[len('START'):], this_args)

        def add_record_item(key, item):
            if key in record.items:
                raise SyntaxError(f'Repeated {key} in {record.type}')
            else:
                record.items[key] = item

        line = nextline(stream)
        while line and not line.startswith('END'):
            if line.startswith('START'):
                record.children.append(
                    BdfRecord.parse_from(stream, first_line=line))
            else:
                if line.startswith('BITMAP'):
                    try:
                        bmp_width, bmp_height, *bmp_off = record.items['BBX']
                    except KeyError:
                        raise SyntaxError(
                            'Expected character BBX before BITMAP')
                    bitmap = BdfBitmap.parse_from(
                        stream, bmp_width, bmp_height, first_line=line)
                    add_record_item('BITMAP', bitmap)
                else:
                    # TODO: If no space is present in `line` it is not a BITMAP line nor a key-value pair, so either:
                    # - The file is malformed
                    # - `BdfBitmap.parse_from()` did not read all lines in the bitmap
                    # Should likely throw an exception in both cases
                    # <KEY> <VALUE1> <VALUE2>...
                    key, values_str = line.split(maxsplit=1)
                    add_record_item(key, BdfProperty(values_str))

            line = nextline(stream)

        expected_end_tag = this_start_tag.replace('START', 'END')
        if line != expected_end_tag:
            got = repr(line) if line else 'EOF'
            raise SyntaxError(f'Expected {expected_end_tag} but got {got}')

        return record

    def __repr__(self) -> str:
        return f'BdfRecord({self.type})'


class BdfFont:
    """A font loaded from a "BDF"."""

    def __init__(self, record: BdfRecord):
        """Inits the font given its FONT record."""

        if record.type != 'FONT':
            raise ValueError('Expected a FONT record in BDF')
        if len(record.children) < 1 or record.children[0].type != 'PROPERTIES':
            raise ValueError('Expected a PROPERTIES record in BDF')
        properties = record.children[0]

        def getval(record, key, default):
            return record.items.get(key, (default,))[0]

        self.bbox = BBox._make(record.items['FONTBOUNDINGBOX'])
        """The font's bounding box (W / H / OX / OY)."""

        self.family = getval(properties, 'FAMILY_NAME', None)
        """Name of the font's family."""
        self.weight = getval(properties, 'WEIGHT_NAME', None)
        """Name of the font's weight."""
        self.logical_name = getval(record, 'FONT', None)
        """Logical (PostScript) name of the font."""
        self.copyright = getval(properties, 'COPYRIGHT', None)
        """Copyright info on the font."""

        # Table for faster lookups
        self._chars = {child.items['ENCODING'][0]: child
                       for child in record.children if child.type == 'CHAR'}


    def render_char(self, code: int) -> List[int]:
        """Renders the character with the given code to a list of bytes.
        (`n` bytes per row, left-to-right, top-to-bottom).

        Returns `None` if the character is missing from the font."""

        char_record = self._chars.get(code, None)
        if not char_record:
            return None

        char_bbox = char_record.items['BBX']
        if char_bbox[0] != self.bbox.w or char_bbox[1] != self.bbox.h:
            raise ValueError(
                f'Character {code} has wrong BBX, font is not monospace!')

        return char_record.items['BITMAP'].data
