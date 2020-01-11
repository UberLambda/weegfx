"""
weegfx/tools/ftfont.py: Font rendering via Freetype

Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
Released under the 3-clause BSD license (see LICENSE)
"""

import os
import sys
import math
from typing import Tuple, List
import freetype as ft  # pip3 install freetype-py
import numpy as np  # pip3 install numpy

from font import row_width, BBox

class FTFont:
    """A TTF/OTF/other font loaded via Freetype."""


    def _guesstimate_size(self, width: int, dpi: int, max_iters: int = 10) -> Tuple[int, int]:
        """Iteratively change `_font` size until the horizontal advance of a 'M' bitmap (roughly)
        matches with the desired one."""
        # TODO: Base sizes off a different character? - doing everything in em here

        ft_size = 1.0
        for i in range(max_iters):
            self._font.set_char_size(int(math.ceil(ft_size * 64)), 0, dpi, 0)
            glyph_width = self._font.size.max_advance // 64
            if glyph_width == width:
                break
            ft_size *= width / glyph_width

        line_height = (self._font.size.ascender - self._font.size.descender) // 64  # (scaled px height)
        return (glyph_width, line_height)
        

    def __init__(self, path: str, width: int, dpi: int):
        """Loads the font given its filepath, height (in pixels) and target DPI (for hinting)."""

        self._font = ft.Face(path)
        if not self._font.is_fixed_width:
            raise RuntimeError(f'{path} is not a monospace font!')

        # FIXME: This can potentially change the width desired by the user!
        #        A better approach would be to always estimate at a loss and add padding columns if needed
        width, height = self._guesstimate_size(width, dpi)
        ox = 0  # TODO: Use horiBearingX of the capital M?
        oy = self._font.descender // 64

        self.bbox = BBox(w=width, h=height, ox=ox, oy=oy)
        """The font's bounding box (W / H / OX / OY)."""

        self.family = self._font.family_name.decode()
        """Name of the font's family."""
        self.weight = self._font.style_name.decode()
        """Name of the font's weight."""
        self.logical_name = self._font.postscript_name.decode()
        """Logical (PostScript) name of the font."""
        self.copyright = None
        """Copyright info on the font."""


    def render_char(self, code: int) -> List[int]:
        """Renders the character with the given code to a list of bytes.
        (`n` bytes per row, left-to-right, top-to-bottom).

        Returns `None` if the character is missing from the font."""

        self._font.load_char(code, ft.FT_LOAD_RENDER | ft.FT_LOAD_TARGET_MONO)  #< !!
        # FIXME: Return None if glyph could not be loaded properly
        glyph = self._font.glyph

        #assert glyph.metrics.horiAdvance // 64 == self.bbox.h
        glyph_bmp_bytes = np.array(glyph.bitmap.buffer, dtype=np.uint8).reshape((glyph.bitmap.rows, glyph.bitmap.pitch))
        glyph_bmp = np.unpackbits(glyph_bmp_bytes, axis=1)
        out_bmp = np.zeros((self.bbox.h, row_width(self.bbox.w)), dtype=np.uint8)

        sy = self.bbox.h + self.bbox.oy - glyph.bitmap_top  # quad bottom -> baseline -> glyph top
        ey = sy + glyph.bitmap.rows
        sx = glyph.bitmap_left  # quad left -> glyph left
        ex = sx + glyph.bitmap.width

        # FIXME
        if ex > self.bbox.w:
            print(f"{chr(code)}: clipped {ex - self.bbox.w}px right", file=sys.stderr)
            ex = self.bbox.w
        if ey > self.bbox.h:
            print(f"{chr(code)}: clipped {ey - self.bbox.h}px bottom", file=sys.stderr)
            ey = self.bbox.h
        #print('oy', self.bbox.oy, 'sy', sy, 'ey', ey, 'sx', sx, 'ex', ex)

        out_bmp[sy:ey, sx:ex] = glyph_bmp[:(ey - sy), :(ex - sx)]

        return np.packbits(out_bmp.ravel())
