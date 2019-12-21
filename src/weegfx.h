// weegfx - A small graphics library for embedded systems with low RAM.
// Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
// Released under the 3-clause BSD license (see LICENSE)
#ifndef WEEGFX_H
#define WEEGFX_H

#ifdef __cplusplus
extern "C" {
#endif

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

/// Fills a rectangle with the given color.
/// `color` is a buffer of `bpp` bytes.
void wgfxFillRect(WGFXscreen *self, unsigned x, unsigned y, unsigned w, unsigned h, const void *color);

#ifdef __cplusplus
}
#endif

#endif // WEEGFX_H