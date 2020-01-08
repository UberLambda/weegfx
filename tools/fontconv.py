#!/usr/bin/env python3
"""
weegfx/tools/fontconv.py: Converts monospaced fonts to C header files suitable for weegfx.

Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
Released under the 3-clause BSD license (see LICENSE)
"""

import os
import sys
from argparse import ArgumentParser
from typing import TextIO

import bdf
from font import row_width

MAX_H_COL = 80
"""Maximum column when generating the .h, after which to wrap."""


def emit_mono_font_header(font: 'Font', first_ch: int, last_ch: int, stream: TextIO):
    """Outputs a weegfx C header file storing a character range (`start_ch`..`end_ch`, both inclusive)
    of given font to `stream`. Only accepts monospace fonts!"""

    def normname(name):
        return ''.join(ch if ch.isalnum() else '_' for ch in name)

    def hexbyte(byte):
        return f'0x{byte:02X}'

    if first_ch > last_ch:
        first_ch, last_ch = first_ch, last_ch
    if not (0 <= first_ch <= 255) or not (0 <= last_ch <= 255):
        raise ValueError(
            'Invalid character range (note: non-ASCII chars are not yet supported!)')

    h_varname = f'FONT_{normname(font.family.upper())}_{normname(font.weight.upper())}_{font.bbox.w}_{font.bbox.h}'
    h_guard = h_varname + '_H'

    h_data_size = row_width(font.bbox.w) // 8 * font.bbox.h * (last_ch - first_ch + 1)
    h_start = f"""// Autogenerated by weegfx/tools/fontconv.py
// Only include this file ONCE in the codebase (every translation unit gets its copy of the font data!)
//
// Font: {font.family or '<unknown family>'} {font.bbox.w}x{font.bbox.h} {font.weight or ''}
//       {font.logical_name or '<unknown logical name>'}
//       {font.copyright or '<no copyright info>'}
// Character range: {hexbyte(first_ch)}..{hexbyte(last_ch)} (both inclusive)
#ifndef {h_guard}
#define {h_guard}

static const WGFX_U8 {h_varname}_DATA[{h_data_size}] WGFX_RODATA = {{"""
    print(h_start, file=stream)

    empty_char_bitmap = [0x00] * (row_width(font.bbox.w) // 8 * font.bbox.h)

    for ich in range(first_ch, last_ch + 1):
        char_bitmap = font.render_char(ich)
        if not char_bitmap:
            print(f'Character {ich} missing, zero-filling pixel data',
                  file=sys.stderr)
            char_bitmap = empty_char_bitmap

        print(
            f'    // {hexbyte(ich)} {repr(chr(ich))}{" (MISSING)" * (char_bitmap == empty_char_bitmap)}', end='', file=stream)

        h_col = MAX_H_COL
        for byte in char_bitmap:
            byte_str = hexbyte(byte) + ', '
            h_col += len(byte_str)
            if h_col >= MAX_H_COL:
                print('\n    ', file=stream, end='')
                h_col = 0
            print(byte_str, end='', file=stream)

        print('', file=stream)

    h_end = f"""}};
    
static const WGFXmonoFont {h_varname} WGFX_RODATA = {{
    {font.bbox.w}, {font.bbox.h},
    {hexbyte(first_ch)}, {hexbyte(last_ch)},
    {h_varname}_DATA,
    {row_width(font.bbox.w) // 8 * font.bbox.h}, // = {row_width(font.bbox.w) // 8} * {font.bbox.h}
}};

#endif // {h_guard}"""
    print(h_end, file=stream)


if __name__ == '__main__':
    argp = ArgumentParser(
        description="Converts a font to a C header suitable for weegfx")
    argp.add_argument('-o', '--outfile', type=str, required=False,
                      help="The file to output to (optional; defaults to stdout)")
    argp.add_argument('infile', type=str,
                      help='The font file to convert')
    argp.add_argument('firstch', type=int,
                      help="First character of the range of characters to output (inclusive)")
    argp.add_argument('lastch', type=int,
                      help="Last character of the range of characters to output (inclusive)")

    args = argp.parse_args()

    with open(args.infile, 'r') as infile:
        outfile = open(args.outfile, 'w') if args.outfile else sys.stdout
        with outfile:
            record = bdf.BdfRecord.parse_from(infile, 'FONT')
            font = bdf.BdfFont(record)
            emit_mono_font_header(font, args.firstch, args.lastch, outfile)
