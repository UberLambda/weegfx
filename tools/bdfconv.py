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


MAX_H_COL = 80
"""Maximum column for generated header files, after which to wrap the source code."""


def nextline(stream: TextIO) -> str:
    """Returns the next non-whitespace line in `stream` (stripped) or an empty line on EOF."""
    line = stream.readline()
    while line.isspace() or line.startswith('COMMENT'):
        line = stream.readline()
    return line.strip()


def bdf_width(width: int) -> int:
    """Calculates the width in bits of each row in the BDF from the actual witdth of a character in pixels."""
    # NOTE: Lines in BDF BITMAPs are always stored in multiples of 8 bits
    # (https://stackoverflow.com/a/37944252)
    return -((-width) // 8) * 8


class BdfBitmap:
    def __init__(self, width: int, height: int, data: Iterable[int] = []):
        self.data = list(data)
        self.width = int(width)
        self.bdf_width = bdf_width(self.width)
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


def emit_mono_bdf_header(bdf: BdfRecord, first_ch: int, last_ch: int, stream: TextIO):
    """Outputs a weegfx C header file storing a character range (`start_ch`..`end_ch`, both inclusive)
    of the FONT record `bdf` to `stream`. Only accepts monospace fonts!"""

    def getval(record, key, default):
        return record.items.get(key, (default,))[0]

    def normname(name):
        return ''.join(ch if ch.isalnum() else '_' for ch in name)

    def hexbyte(byte):
        return f'0x{byte:02X}'

    if bdf.type != 'FONT':
        raise ValueError('Expected a FONT record in BDF')

    if first_ch > last_ch:
        first_ch, last_ch = first_ch, last_ch
    if not (0 <= first_ch <= 255) or not (0 <= last_ch <= 255):
        raise ValueError(
            'Invalid character range (note: non-ASCII chars are not yet supported!)')

    if len(bdf.children) < 1 or bdf.children[0].type != 'PROPERTIES':
        raise ValueError('Expected a PROPERTIES record in BDF')
    properties = bdf.children[0]

    font_w, font_h, font_ox, font_oy = bbox = bdf.items['FONTBOUNDINGBOX']
    font_family = getval(properties, 'FAMILY_NAME', '<unknown family>')
    font_weight = getval(properties, 'WEIGHT_NAME', '')

    h_varname = f'FONT_{normname(font_family.upper())}_{normname(font_weight.upper())}_{font_w}_{font_h}'
    h_guard = h_varname + '_H'

    h_start = f"""// Autogenerated by weegfx/tools/bdfconv.py
// Only include this file ONCE in the codebase (every translation unit gets its copy of the font data!)
//
// Font: {getval(properties, 'FAMILY_NAME', '<unknown family>')} {font_w}x{font_h} {getval(properties, 'WEIGHT_NAME', '')}
//       {getval(bdf, 'FONT', '<unknown logical name>')}
//       Copyright {getval(properties, 'COPYRIGHT', '<no copyright info>')}
// Character range: {hexbyte(first_ch)}..{hexbyte(last_ch)} (both inclusive)
#ifndef {h_guard}
#define {h_guard}

static const WGFX_U8 {h_varname}_DATA[] WGFX_RODATA = {{"""
    print(h_start, file=stream)

    # Table for faster lookups
    font_chars = {child.items['ENCODING'][0]: child
                  for child in bdf.children if child.type == 'CHAR'}

    bytes_per_row = bdf_width(font_w) // 8
    empty_char_bitmap = [0x00] * font_h * bytes_per_row

    for ich in range(first_ch, last_ch + 1):
        char_record = font_chars.get(ich, None)
        if char_record:
            char_bbox = char_record.items['BBX']
            if char_bbox[0] != font_w or char_bbox[1] != font_h:
                raise ValueError(
                    f'Character {ich} has wrong BBX, font is not monospace!')
            char_bitmap = char_record.items['BITMAP']
        else:
            print(f'Character {ich} missing, zero-filling pixel data',
                  file=sys.stderr)
            char_bitmap = empty_char_bitmap

        print(
            f'    // {hexbyte(ich)} {repr(chr(ich))}{" (MISSING)" * (not char_record)}', end='', file=stream)

        h_col = MAX_H_COL
        for byte in char_bitmap.data:
            if h_col >= MAX_H_COL:
                print('\n    ', file=stream, end='')
                h_col = 0
            print(hexbyte(byte) + ', ', end='', file=stream)

        print('', file=stream)

    h_end = f"""}};
    
static const WGFXmonoFont {h_varname} WGFX_RODATA = {{
    {font_w}, {font_h},
    {hexbyte(first_ch)}, {hexbyte(last_ch)},
    &{h_varname}_DATA,
    {bdf_width(font_w) * font_h}, // = {bdf_width(font_w)} * {font_h}
}};

#endif // {h_guard}"""
    print(h_end, file=stream)


if __name__ == '__main__':
    argp = ArgumentParser(
        description="Converts a BDF font to a C header suitable for weegfx")
    argp.add_argument('-o', '--outfile', type=str, required=False,
                      help="The file to output to (optional; defaults to stdout)")
    argp.add_argument('infile', type=str,
                      help='The BDF file to convert')
    argp.add_argument('firstch', type=int,
                      help="First character of the range of characters to output (inclusive)")
    argp.add_argument('lastch', type=int,
                      help="Last character of the range of characters to output (inclusive)")

    args = argp.parse_args()

    with open(args.infile, 'r') as infile:
        outfile = open(args.outfile, 'w') if args.outfile else sys.stdout
        with outfile:
            bdf = BdfRecord.parse_from(infile, 'FONT')
            emit_mono_bdf_header(bdf, args.firstch, args.lastch, outfile)
