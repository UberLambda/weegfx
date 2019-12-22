#!/usr/bin/env python3
"""
weegfx/tools/bdfconv.py: Converts monospaced fonts in BDF format to C header files suitable for weegfx.

Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
Released under the 3-clause BSD license (see LICENSE)
"""

import os
import sys
from argparse import ArgumentParser
from typing import TextIO, Iterable


class BdfRecord:
    def __init__(self, type: str, args: Iterable):
        self.type = type
        self.args = list(args)
        self.lines = []
        self.children = []

    def __repr__(self) -> str:
        return f'BdfRecord({self.type})'


class Bdf:
    def __init__(self, infile: TextIO):
        """Parses a BDF font."""
        self.infile = infile
        self.font_record = self._parse_record('FONT')

    def _nextline(self):
        line = self.infile.readline()
        while line.isspace() or line.startswith('COMMENT'):
            line = self.infile.readline()
        return line.strip()

    def _parse_record(self, expected_type: str = None, first_line: str = None) -> BdfRecord:
        first_line = first_line or self._nextline()

        expected_start_tag = 'START' + expected_type if expected_type else ''
        if not first_line.startswith(expected_start_tag):
            raise SyntaxError(
                f'Expected {expected_start_tag} but found {first_line}')

        this_start_tag, *this_args = first_line.split()
        record = BdfRecord(this_start_tag[len('START'):], this_args)

        line = self._nextline()
        while line and not line.startswith('END'):
            if line.startswith('START'):
                record.children.append(self._parse_record(first_line=line))
            else:
                record.lines.append(line)
            line = self._nextline()

        expected_end_tag = this_start_tag.replace('START', 'END')
        if line != expected_end_tag:
            got = repr(line) if line else 'EOF'
            raise SyntaxError(f'Expected {expected_end_tag} but got {got}')

        return record


if __name__ == '__main__':
    with open('/tmp/test.bdf') as infile:
        bdf = Bdf(infile)
        print(bdf.font_record.children[0].lines)
        print(bdf.font_record.children[1].lines)
