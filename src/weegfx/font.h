// weegfx/font.h - Font-related definitions.
// Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
// Released under the 3-clause BSD license (see LICENSE)
#ifndef WEEGFX_FONT_H
#define WEEGFX_FONT_H

#include "types.h"

/// A monospaced bitmap font.
typedef struct
{
    /// Width and height of each character in the font.
    unsigned width, height;

    /// First and last characters in the font (both inclusive).
    char firstChar, lastChar;

    /// The stored character data.
    ///
    /// Stored as a contiguous array, indexed by:
    /// 1) (Character - `firstChar`) index
    /// 2) Pixel row (top-to-bottom)
    /// 3) Row pixels (one pixel per bit)
    /// Widths are rounded up to the nearest multiple of 8 bits
    /// (e.g.: for a font of width 13, 16 bits are used for each row).
    const WGFX_U8 *data;

} WGFXmonoFont;

#endif // WEEGFX_FONT_H