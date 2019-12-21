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
typedef void (*WGFXreadRectPFN)(WGFX_SIZET x, WGFX_SIZET y, WGFX_SIZET w, WGFX_SIZET h, WGFX_U8 *buf, void *userPtr);

/// A function that copies a rectangle of pixels from `buf` to the screen.
typedef void (*WGFXwriteRectPFN)(WGFX_SIZET x, WGFX_SIZET y, WGFX_SIZET w, WGFX_SIZET h, const WGFX_U8 *buf, void *userPtr);

/// An instance of weegfx.
typedef struct
{
    /// Width and height of the screen in pixels.
    WGFX_SIZET width, height;

    /// Bytes per pixel as stored in the screen framebuffer and scratch buffer.
    unsigned bpp;

    /// Size in bytes of the scratch buffer.
    /// WARNING: This should be a multiple of `bpp`!
    WGFX_SIZET sbSize;

    /// The scratch buffer; must be at least `fbSize` bytes in size.
    WGFX_U8 *sbData;

    /// Used to read pixel data from the actual screen.
    WGFXreadRectPFN readRect;

    /// Used to write pixel data to the actual screen.
    WGFXwriteRectPFN writeRect;

    /// User data pointer passed to `readRect` and `writeRect`.
    void *userPtr;

} WGFXscreen;

#ifdef __cplusplus
}
#endif

#endif // WEEGFX_H