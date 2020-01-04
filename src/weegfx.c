// weegfx - A small graphics library for embedded systems with low RAM.
// Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
// Released under the 3-clause BSD license (see LICENSE)
#include "weegfx.h"

#include "weegfx/base.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void wgfxFillRect(WGFXscreen *self, unsigned x, unsigned y, unsigned w, unsigned h, const WGFXcolor color)
{
    if(w == 0 || h == 0) return;

    const unsigned endX = x + w, endY = y + h;

#ifndef WGFX_NO_CLIPPING
    if(x >= self->width) x = self->width - 1;
    if(endX >= self->width) w = self->width - x;
    if(y >= self->height) y = self->height - 1;
    if(endY >= self->height) h = self->height - y;
#endif

    const WGFX_SIZET xferCount = w * h;
    const WGFX_SIZET scratchSizeB = self->scratchSize * self->bpp;
    const int rectFitsScratch = self->scratchSize >= xferCount;

    const WGFX_SIZET fillCount = rectFitsScratch ? xferCount : self->scratchSize;
    for(size_t i = 0, ib = 0; i < fillCount; i++, ib += self->bpp)
    {
        // TODO PERFORMANCE: Replace this memcpy with a for loop? (`bpp` will always be small)
        WGFX_MEMCPY(&self->scratchData[ib], color, self->bpp);
    }

    self->beginWrite(x, y, w, h, self->userPtr);
    if(rectFitsScratch)
    {
        // Fill whole rect at once
        self->write(self->scratchData, scratchSizeB, self->userPtr);
    }
    else
    {
        // Fill rect in chunks
        const WGFX_SIZET xferSizeB = xferCount * self->bpp;
        WGFX_SIZET chunkSizeB = scratchSizeB;
        for(WGFX_SIZET sentB = 0; sentB < xferSizeB; sentB += chunkSizeB)
        {
            self->write(self->scratchData, chunkSizeB, self->userPtr);
            chunkSizeB = MIN(chunkSizeB, xferSizeB - sentB);
        }
    }
    self->endWrite(self->userPtr);
}

/// <string.h>-less strlen
inline static unsigned stringLength(const char *string)
{
    const char *iCh = string;
    while(*iCh)
    {
        iCh++;
    }
    return (unsigned)(iCh - string);
}

/// Renders a `width * height` rectangle of a `font` character to a data `buffer`,
/// scaled by `scale`, and with given foreground and background colors.
/// Note that `the `width` and `height` passed to this function are expected to be scaled by `scale` as needed.
/// `rowStride` is the offset in bytes between rows of `buffer`.
///
/// Returns the pointer into `buffer` at the character's bottom-right corner.
/// Does NOT even try to perform any clipping or bounds checking!
WGFX_FORCEINLINE static WGFX_U8 *writeMonoChar(
    char ch,
    WGFX_U8 *buffer, unsigned bpp, unsigned rowStride,
    const WGFXmonoFont *font, unsigned scale, unsigned width, unsigned height,
    const WGFXcolor fgColor, const WGFXcolor bgColor)
{
    WGFX_U8 *bufPtr = buffer;
    const int hasChar = font->firstChar <= ch && ch <= font->lastChar;
    if(hasChar)
    {
        const WGFX_U8 *data = font->data + ((WGFX_SIZET)(ch - font->firstChar) * font->charDataStride);
        WGFX_U8 dataByte;
        unsigned dataByteBit = 0; // Which bit into the current data byte

        const unsigned charRowStride = width * bpp; // Length in bytes of a character row

        for(unsigned row = 0; row < height; row += scale)
        {
            dataByte = WGFX_RODATA_READU8(data);
            dataByteBit = 0;
            bufPtr = buffer;

            unsigned col = 0;
            for(unsigned dataBit = 0; dataBit < font->width; dataBit++)
            {
                // Draw pixels only up to `width`, but seek in the memory buffer up to `font->width`!
                if(col < width)
                {
                    const int pixelOn = dataByte & 0x80;
                    dataByte <<= 1;

                    for(unsigned i = 0; i < scale; i++) //< (to upscale horizontally)
                    {
                        // TODO PERFORMANCE: Replace memcpy with a simple for loop?
                        // TODO PERFORMANCE: Replace the ternary operator with something else?
                        WGFX_MEMCPY(bufPtr, pixelOn ? fgColor : bgColor, bpp);
                        bufPtr += bpp;
                    }
                }
                col += scale;

                // (the sequence below should be slightly faster than using `dataByteBit % 8`)
                dataByteBit++;
                if(dataByteBit == 8)
                {
                    data++;
                    dataByte = WGFX_RODATA_READU8(data);
                    dataByteBit = 0;
                }
            }

            if(dataByteBit != 0)
            {
                data++; // Skip trailing zeroes for the byte
            }
            buffer += rowStride; // End of row

            for(unsigned i = 1; i < scale; i++) //< (to upscale vertically)
            {
                WGFX_MEMCPY(buffer, buffer - rowStride, charRowStride);
                buffer += rowStride; // End of row
            }
        }
    }
    else
    {
        for(unsigned row = 0; row < height; row++)
        {
            bufPtr = buffer;
            for(unsigned col = 0; col < width; col++)
            {
                // TODO PERFORMANCE: Replace memcpy with a simple for loop?
                WGFX_MEMCPY(bufPtr, bgColor, bpp);
                bufPtr += bpp;
            }
            buffer += rowStride;
        }
    }

    return buffer;
}

int wgfxDrawTextMono(WGFXscreen *self, const char *string, unsigned length, unsigned *x, unsigned *y,
                     const WGFXmonoFont *font, unsigned scale, const WGFXcolor fgColor, const WGFXcolor bgColor, WGFXwrapMode wrapMode)
{
    const unsigned startX = *x, startY = *y;
    scale = (scale > 1) ? scale : 1;
    const unsigned charWidth = font->width * scale, charHeight = font->height * scale;

    const unsigned pixelsPerChar = charWidth * charHeight;
    const unsigned maxScratchChars = self->scratchSize / pixelsPerChar;
    if(maxScratchChars == 0)
    {
        // Not enough memory to fit even one char
        return 0;
    }
    const unsigned maxScratchWidth = maxScratchChars * charWidth;

    unsigned lineWidth = 0, lineHeight = charHeight; // Width/height of scratch buffer rect for this line

    length = length == 0 ? stringLength(string) : length;
    const char *const strEnd = string + length;
    const char *iCh = string;
    while(iCh < strEnd)
    {
        // Read entire line
        const char *const lineStart = iCh;
        while(*iCh != '\n' && iCh < strEnd)
        {
            iCh++;
        }
        const char *const lineEnd = iCh;
        const WGFX_SIZET charsThisLine = lineEnd - lineStart;

#ifndef WGFX_NO_CLIPPING
        const unsigned nextLineY = *y + charHeight;

        if(*y >= self->height)
        {
            // Line outside screen - exit
            return 1;
        }
        else if(nextLineY >= self->height)
        {
            // Line only partially visible - clip
            lineHeight = charHeight - (nextLineY - self->height);
        }
#endif

        iCh = lineStart;

        const unsigned nChunks = (charsThisLine - 1) / maxScratchChars + 1;
        int lastCharClipped = 0; // Was the last character drawn cut off?
        for(unsigned i = 0; i < nChunks; i++)
        {
#ifndef WGFX_NO_CLIPPING
            if(*x >= self->width && (wrapMode & WGFX_WRAP_NEWLINE))
            {
                // This chunk of this line is offscreen; go to the next newline (if any)
                lastCharClipped = 0;
                iCh = lineEnd;
                break;
            }
#endif

            WGFX_U8 *chunkBuffer = self->scratchData;
            const unsigned nCharsThisChunk = MIN(maxScratchChars, charsThisLine - (iCh - lineStart));
            const unsigned maxChunkWidth = nCharsThisChunk * charWidth;       // Hypothetical maximum width for this chunk
            const unsigned chunkWidth = MIN(maxChunkWidth, self->width - *x); // Actual width of this chunk
            const unsigned chunkRowStride = chunkWidth * self->bpp;

            // Render as many whole characters as possible
            unsigned xRight = 0; // End X of the current char, relative to scratch buffer X=0
            if(chunkWidth >= charWidth)
            {
                const unsigned charStride = charWidth * self->bpp; // Offset to go right to the top-left corner of next char

                for(; xRight < chunkWidth; xRight += charWidth)
                {
                    writeMonoChar(*iCh, chunkBuffer, self->bpp, chunkRowStride, font, scale, charWidth, lineHeight, fgColor, bgColor);
                    chunkBuffer += charStride;
                    iCh++;
                }
            }

            lastCharClipped = xRight > chunkWidth || xRight == 0;
            if(lastCharClipped)
            {
                // Either the last char's end X overshoot the end of the chunk,
                // or we couldn't fit any character in it
                // -> need to draw a partially-clipped char or blank its area (depending on wrap mode)
                const unsigned lastCharWidth = chunkWidth - (xRight - charWidth);

                if(!(wrapMode & WGFX_WRAP_RIGHT))
                {
                    // Clip last character
                    writeMonoChar(*iCh, chunkBuffer, self->bpp, chunkRowStride, font, scale, lastCharWidth, lineHeight, fgColor, bgColor);
                    iCh++;
                }
                else
                {
                    // Wrap right: instead of drawing a partially clipped char at the end of the line, the character will
                    // be drawn at the start of the next line.
                    // Still need to fill the rectangle with `bgColor` to prevent artefacts!
                    for(unsigned iRow = 0; iRow < lineHeight; iRow++)
                    {
                        WGFX_U8 *bufPtr = chunkBuffer;
                        for(unsigned iCol = 0; iCol < lastCharWidth; iCol++)
                        {
                            // TODO: Replace the memcpy with a simple for loop for performance?
                            WGFX_MEMCPY(bufPtr, bgColor, self->bpp);
                            bufPtr += self->bpp;
                        }
                        chunkBuffer += chunkRowStride;
                    }
                }
            }

            // Draw the whole scratch buffer to the screen when:
            // - Scratch buffer full
            // - '\n' reached
            // - End of string reached
            self->beginWrite(*x, *y, chunkWidth, lineHeight, self->userPtr);
            self->write(self->scratchData, chunkWidth * lineHeight * self->bpp, self->userPtr);
            self->endWrite(self->userPtr);
            *x += chunkWidth;

            if(lastCharClipped && (wrapMode & WGFX_WRAP_RIGHT))
            {
                // Wrap and continue next line from `iCh`
                *x = startX;
                *y += lineHeight;
                break;
            }
        }

        if(*iCh == '\n')
        {
            if(wrapMode & WGFX_WRAP_NEWLINE)
            {
                *x = startX;
                *y += lineHeight;
            }
            else
            {
                // Need to draw the newline as a normal character
                // FIXME IMPLEMENT!
                //writeMonoChar('\n', charBuffer, self->bpp, charRowStride, font, font->width, font->height, fgColor, bgColor);
                //self->writeRect(*x, *y, font->width, font->height, self->scratchData, self->userPtr);
            }
        }
        if(!lastCharClipped)
        {
            iCh++; // Skip the '\n' or end char
        }
        // else continue next line from the clipped char
    }

    return 1;
}

void wgfxDrawBitmap(WGFXscreen *self, const WGFX_U8 *image, unsigned imgW, unsigned imgH,
                    unsigned x, unsigned y, unsigned w, unsigned h, WGFXbitmapFlags flags)
{
    w = MIN(imgW, w);
    h = MIN(imgH, h);
    const unsigned endX = x + w, endY = y + h;

#ifndef WGFX_NO_CLIPPING
    if(x >= self->width)
    {
        return;
    }
    else if(endX >= self->width)
    {
        w = self->width - x;
    }

    if(y >= self->height)
    {
        return;
    }
    else if(endY >= self->height)
    {
        h = self->height - y;
    }
#endif

    const int rodata = flags & WGFX_BITMAP_RODATA;
    const WGFX_SIZET scratchSizeB = self->scratchSize * self->bpp;
    const WGFX_SIZET rectSize = w * h, rectSizeB = rectSize * self->bpp;

    self->beginWrite(x, y, w, h, self->userPtr);

    if(!rodata && w == imgW && h == imgH)
    {
        // Can write the whole image at once directly
        self->write(image, rectSize, self->userPtr);
    }
    else
    {
        // Need to draw the image row-by-row...
        const WGFX_SIZET imageRowStride = imgW * self->bpp;
        WGFX_SIZET bytesPerChunk = MIN(scratchSizeB, w * self->bpp);
        if(rodata)
        {
            for(WGFX_SIZET iByte = 0; iByte < rectSizeB; iByte += bytesPerChunk)
            {
                bytesPerChunk = MIN(bytesPerChunk, rectSizeB - iByte);
                WGFX_RODATA_MEMCPY(self->scratchData, image, bytesPerChunk);
                self->write(self->scratchData, bytesPerChunk, self->userPtr);
                image += imageRowStride;
            }
        }
        else
        {
            for(WGFX_SIZET iByte = 0; iByte < rectSizeB; iByte += bytesPerChunk)
            {
                bytesPerChunk = MIN(bytesPerChunk, rectSizeB - iByte);
                self->write(image, bytesPerChunk, self->userPtr);
                image += imageRowStride;
            }
        }
    }

    self->endWrite(self->userPtr);
}
