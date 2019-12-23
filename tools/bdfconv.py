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


def nextline(stream: TextIO) -> str:
    """Returns the next non-whitespace line in `stream` (stripped) or an empty line on EOF."""
    line = stream.readline()
    while line.isspace() or line.startswith('COMMENT'):
        line = stream.readline()
    return line.strip()


class BdfRecord:
    def __init__(self, type: str, args: Iterable):
        self.type = type
        self.args = list(args)
        self.lines = []
        self.children = []

    @staticmethod
    def parse_from(stream: TextIO, expected_type: str = None, first_line: str = None) -> "BdfRecord":
        """Parses a BDF record from the given stream.
        If `expected_type` is present, throws an exception if the record is not of the given type.
        If `first_line` is present, uses it instead of fetching a first line from the stream.
        """
        first_line = first_line or nextline(stream)

        expected_start_tag = 'START' + expected_type if expected_type else ''
        if not first_line.startswith(expected_start_tag):
            raise SyntaxError(
                f'Expected {expected_start_tag} but found {first_line}')

        this_start_tag, *this_args = first_line.split()
        record = BdfRecord(this_start_tag[len('START'):], this_args)

        line = nextline(stream)
        while line and not line.startswith('END'):
            if line.startswith('START'):
                record.children.append(
                    BdfRecord.parse_from(stream, first_line=line))
            else:
                record.lines.append(line)
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
        print(bdf.children[0].lines)
        print(bdf.children[1].lines)
