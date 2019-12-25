// weegfx - A small graphics library for embedded systems with low RAM.
// Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
// Released under the 3-clause BSD license (see LICENSE)
#ifndef WEEGFX_H
#define WEEGFX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "weegfx/base.h" // For WGFX_RODATA
#include "weegfx/font.h"
#include "weegfx/types.h"

/// A function that reads a rectangle of pixels from the screen to `buf`.
typedef void (*WGFXreadRectPFN)(unsigned x, unsigned y, unsigned w, unsigned h, WGFX_U8 *buf, void *userPtr);

/// A function that copies a rectangle of pixels from `buf` to the screen.
typedef void (*WGFXwriteRectPFN)(unsigned x, unsigned y, unsigned w, unsigned h, const WGFX_U8 *buf, void *userPtr);

/// An instance of weegfx.
typedef struct
{
    /// Width and height of the screen in pixels.
    unsigned width, height;

    /// Bytes per pixel as stored in the screen framebuffer and scratch buffer.
    unsigned bpp;

    /// Size in _pixels_ of the scratch buffer.
    ///
    /// Due to the inner workings of the library, the ideal `scratchSize` is:
    /// - A multiple of the screen height
    /// - Enough to contain at least one character of the biggest font used
    WGFX_SIZET scratchSize;

    /// The scratch buffer; must be at least `fbSize` bytes in size.
    WGFX_U8 *scratchData;

    /// Used to read pixel data from the actual screen.
    WGFXreadRectPFN readRect;

    /// Used to write pixel data to the actual screen.
    WGFXwriteRectPFN writeRect;

    /// User data pointer passed to `readRect` and `writeRect`.
    void *userPtr;

} WGFXscreen;

/// A color in weegfx, i.e. an array of `bpp` bytes used to represent a single pixel.
typedef void *WGFXcolor;

/// A bitmask of wrapping modes for text.
typedef enum
{
    WGFX_WRAP_NEWLINE = 0x1, ///< Wrap on '\n'? (treat '\n' as any other character otherwise)
    WGFX_WRAP_RIGHT = 0x2,   ///< Wrap on right screen edge? (clip otherwise)
    WGFX_WRAP_ALL = WGFX_WRAP_NEWLINE | WGFX_WRAP_RIGHT,
} WGFXwrapMode;

/// Fills a rectangle with the given color.
/// `color` is a buffer of `bpp` bytes.
void wgfxFillRect(WGFXscreen *self, unsigned x, unsigned y, unsigned w, unsigned h, const WGFXcolor color);

/// Draws a string in monospace font. Overwrites the background!
/// If `length` is 0, `strlen(string)` is used.
/// `fgColor` and `bgColor` represent the text color and background color respectively.
/// `*x` and `*y` is the position of the top-left corner of the first letter of the text;
/// they are set to the position of the top-right corner of the last letter when the function returns.
///
/// Returns false on failure - usually because `scratchSize` is not enough to hold at least one character of the text...
int wgfxDrawTextMono(WGFXscreen *self, const char *string, unsigned length, unsigned *x, unsigned *y,
                     const WGFXmonoFont *font, const WGFXcolor fgColor, const WGFXcolor bgColor, WGFXwrapMode wrapMode);

#ifdef __cplusplus
}
#endif

#endif // WEEGFX_H