"""
weegfx/tools/font.py: Font-related types and functions

Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
Released under the 3-clause BSD license (see LICENSE)
"""

from collections import namedtuple

def row_width(width: int) -> int:
    """Calculates the width in bits of each row in the output font bitmaps from the actual witdth of a character in pixels."""
    # NOTE: Lines in BDF BITMAPs are always stored in multiples of 8 bits
    # (https://stackoverflow.com/a/37944252)
    return -((-width) // 8) * 8

BBox = namedtuple('BBox', 'w h ox oy')
"""The bounding box of a font's characters (width / height / origin X / origin Y)."""
