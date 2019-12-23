#!/usr/bin/env python3
"""
weegfx/tools/bdfconv.py: Converts monospaced fonts in BDF format to C header files suitable for weegfx.

Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
Released under the 3-clause BSD license (see LICENSE)
"""

import os
import sys
import re
from argparse import ArgumentParser
from typing import TextIO, Iterable, Any, Callable


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
        self.height = int(height)

        # NOTE: Lines in BDF BITMAPs are always stored in multiples of 8 bits
        # (https://stackoverflow.com/a/37944252)
        self.bdf_width = -((-self.width) // 8) * 8

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


class BdfProperty:
    VALUE_RE = re.compile(
        r'\s*(?:(?P<int>-?\d+)|(?P<quoted>"[^"]*")|(?P<unquoted>.+))')

    def __init__(self, key: str, values_str: str):
        """Inits a property from its key (name) and a string representing its value(s).
        Value(s) are parsed as either ints or strings."""

        self.key = str(key)
        """Name/type of the property."""
        self.value = self._parse_value(values_str)
        """Either a tuple of strings/integers or a single string/integer."""

    def _parse_value(self, value_str: str) -> Any:
        values = []
        for match in self.VALUE_RE.finditer(value_str):
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

        return tuple(values) if len(values) != 1 else values[0]

    def __repr__(self) -> str:
        return f'{repr(self.value)}'


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
                        bmp_width, bmp_height, \
                            *bmp_off = record.items['BBX'].value
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
                    add_record_item(key, BdfProperty(key, values_str))

            line = nextline(stream)

        expected_end_tag = this_start_tag.replace('START', 'END')
        if line != expected_end_tag:
            got = repr(line) if line else 'EOF'
            raise SyntaxError(f'Expected {expected_end_tag} but got {got}')

        return record

    def __repr__(self) -> str:
        return f'BdfRecord({self.type})'


if __name__ == '__main__':
    with open('/tmp/test.bdf') as infile:
        bdf = BdfRecord.parse_from(infile, 'FONT')
        print(bdf.children[0].items)
        print(bdf.children[1].items)
