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

/// A function that sets the address window of the screen and commences a write to it.
/// See `WGFXscreen::beginWrite`.
typedef void (*WGFXbeginWritePFN)(unsigned x, unsigned y, unsigned w, unsigned h, void *userPtr);

/// A function that writes `size` bytes of pixel data to the screen.
/// See `WGFXscreen::write`.
typedef void (*WGFXwritePFN)(const WGFX_U8 *buf, WGFX_SIZET size, void *userPtr);

/// A function that ends a screen write.
/// See `WGFXscreen::endWrite`.
typedef void (*WGFXendWritePFN)(void *userPtr);

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

    /// Used by the library to begin writing to the screen.
    /// It gets passed the X, Y (top-left) and width, height of the rectangle of
    /// the screen that the library wants to write to, and it should tell the screen
    /// to start writing at that location.
    WGFXbeginWritePFN beginWrite;

    /// Used by the library to write pixel data to the screen.
    /// After a `beginWrite()`, `write()` is called enough times to cover the whole
    /// pixel area (address window) specified in `beginWrite()`.
    ///
    /// The passed buffer data is guaranteed to contain a multiple of `bpp` bytes,
    /// with no partial pixel passed (i.e. the first byte of the buffer is the
    /// first byte of a pixel and the last byte of the buffer is the last byte of a pixel).
    WGFXwritePFN write;

    /// Used by the library to end a write to the screen.
    /// This is called after `beginWrite()` and `write()`[s] have happened.
    WGFXendWritePFN endWrite;

    /// User data pointer passed to `beginWrite`, `write` and `endWrite`.
    void *userPtr;

} WGFXscreen;

/// A color in weegfx, i.e. an array of `bpp` bytes used to represent a single pixel.
typedef const void *WGFXcolor;

/// A bitmask of wrapping modes for text.
typedef enum
{
    WGFX_WRAP_NONE = 0x0,    ///< Never wrap (= clip right)
    WGFX_WRAP_NEWLINE = 0x1, ///< Wrap on '\n'? (treat '\n' as any other character otherwise)
    WGFX_WRAP_RIGHT = 0x2,   ///< Wrap on right screen edge? (clip otherwise)
    WGFX_WRAP_ALL = WGFX_WRAP_NEWLINE | WGFX_WRAP_RIGHT,
} WGFXwrapMode;

/// A bitmask of bitmap drawing flags.
typedef enum
{
    WGFX_BITMAP_RODATA = 0x1, ///< The bitmap data is in `WGFX_RODATA` and requires special care when reading.
} WGFXbitmapFlags;

/// Fills a rectangle with the given color.
/// `color` is a buffer of `bpp` bytes.
void wgfxFillRect(WGFXscreen *self, unsigned x, unsigned y, unsigned w, unsigned h, const WGFXcolor color);

/// Draws a string in monospace font. Overwrites the background!
/// If `length` is 0, `strlen(string)` is used.
/// `fgColor` and `bgColor` represent the text color and background color respectively.
/// `*x` and `*y` is the position of the top-left corner of the first letter of the text;
/// they are set to the position of the top-right corner of the last letter when the function returns.
/// If `scale` is `> 1`, the font will be upscaled (nearest neighbour) by that factor before drawing.
///
/// Returns false on failure - usually because `scratchSize` is not enough to hold at least one character of the text...
int wgfxDrawTextMono(WGFXscreen *self, const char *string, unsigned length, unsigned *x, unsigned *y,
                     const WGFXmonoFont *font, unsigned scale, const WGFXcolor fgColor, const WGFXcolor bgColor, WGFXwrapMode wrapMode);

/// Estimates the `w`idth and `h`eight of the bounding rectangle of a string as it were drawn by `wgfxDrawTextMono()`.
/// Applies wrapping and clipping according to `wrapMode`.
/// Note that the outputted `w` and `h` might describe a rectangle that goes beyond the screen's bounds!
void wgfxTextBoundsMono(WGFXscreen *self, const char *string, unsigned length, unsigned x, unsigned y, unsigned *w, unsigned *h,
                        const WGFXmonoFont *font, unsigned scale, WGFXwrapMode wrapMode);

/// Draws (at most) `w * h` pixels of `image` (that is a `imgW * imgH` bitmap) to the screen, at position `x`,`y`.
///
/// Data in `image` is contiguous top-to-bottom, left-to-right, `bpp` bytes per pixel.
/// If `flags & WGFX_BITMAP_RODATA` the image is assumed to be `WGFX_RODATA`, so its pixel data is loaded to memory
/// (in chunks) before drawing it.
void wgfxDrawBitmap(WGFXscreen *self, const WGFX_U8 *image, unsigned imgW, unsigned imgH,
                    unsigned x, unsigned y, unsigned w, unsigned h,
                    WGFXbitmapFlags flags);

#ifdef __cplusplus
}
#endif

#endif // WEEGFX_H