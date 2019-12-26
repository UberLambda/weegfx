// weegfx - A small graphics library for embedded systems with low RAM.
// Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
// Released under the 3-clause BSD license (see LICENSE)
#include "weegfx.h"

#include "weegfx/base.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void wgfxFillRect(WGFXscreen *self, unsigned x, unsigned y, unsigned w, unsigned h, const WGFXcolor color)
{
    const unsigned endX = x + w, endY = y + h;

#ifndef WGFX_NO_CLIPPING
    if(x >= self->width) x = self->width - 1;
    if(endX >= self->width) w = self->width - x;
    if(y >= self->height) y = self->height - 1;
    if(endY >= self->height) h = self->height - y;
#endif

    const WGFX_SIZET xferSize = w * h;
    const int rectFitsScratch = self->scratchSize >= xferSize;

    const WGFX_SIZET fillSize = rectFitsScratch ? xferSize : self->scratchSize;
    for(size_t i = 0, ib = 0; i < fillSize; i++, ib += self->bpp)
    {
        // TODO PERFORMANCE: Replace this memcpy with a for loop? (`bpp` will always be small)
        WGFX_MEMCPY(&self->scratchData[ib], color, self->bpp);
    }

    if(rectFitsScratch)
    {
        // Fill whole rect at once
        self->writeRect(x, y, w, h, self->scratchData, self->userPtr);
    }
    else
    {
        // Fill rect left-to-right, top-to-bottom with smaller rects

        unsigned fillH = MIN(h, self->scratchSize); //< min(scratchBuf max height, height left to fill)
        unsigned fillW;
        for(unsigned iy = y; iy < endY; iy += fillH)
        {
            fillH = MIN(fillH, endY - iy); //< min(scratchBuf max height, height left to fill)

            fillW = MIN(self->scratchSize / fillH, w); //< min(scratchBuf max width, total width to fill)
            for(unsigned ix = x; ix < endX; ix += fillW)
            {
                fillW = MIN(fillW, endX - ix); //< min(scratchBuf max width, width left to fill)
                self->writeRect(ix, iy, fillW, fillH, self->scratchData, self->userPtr);
            }
        }
    }
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

/// Returns the pointer into `buffer` at the character's bottom-right corner.
/// Does NOT even try to perform any clipping or bounds checking!
WGFX_FORCEINLINE static WGFX_U8 *writeMonoChar(
    char ch,
    WGFX_U8 *buffer, unsigned bpp, unsigned rowStride,
    const WGFXmonoFont *font, unsigned width, unsigned height,
    const WGFXcolor fgColor, const WGFXcolor bgColor)
{
    const int hasChar = font->firstChar <= ch && ch <= font->lastChar;
    if(hasChar)
    {
        const WGFX_U8 *data = font->data + ((WGFX_SIZET)(ch - font->firstChar) * font->charDataStride);
        WGFX_U8 dataByte;
        unsigned dataByteBit = 0;
        for(unsigned row = 0; row < height; row++)
        {
            dataByte = WGFX_RODATA_READU8(data);
            dataByteBit = 0;

            for(unsigned colBit = 0; colBit < width; colBit++)
            {
                const int pixelOn = dataByte & 0x80;
                dataByte <<= 1;
                // TODO PERFORMANCE: Replace memcpy with a simple for loop?
                // TODO PERFORMANCE: Replace the ternary operator with something else?
                WGFX_MEMCPY(buffer, pixelOn ? fgColor : bgColor, bpp);
                buffer += bpp;

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
        }
    }
    else
    {
        for(unsigned row = 0; row < height; row++)
        {
            for(unsigned col = 0; col < width; col++)
            {
                // TODO PERFORMANCE: Replace memcpy with a simple for loop?
                WGFX_MEMCPY(buffer, bgColor, bpp);
                buffer += bpp;
            }
            buffer += rowStride;
        }
    }

    return buffer;
}

int wgfxDrawTextMono(WGFXscreen *self, const char *string, unsigned length, unsigned *x, unsigned *y,
                     const WGFXmonoFont *font, const WGFXcolor fgColor, const WGFXcolor bgColor, WGFXwrapMode wrapMode)
{
    const unsigned startX = *x, startY = *y;

    const unsigned pixelsPerChar = font->width * font->height;
    const unsigned maxScratchChars = self->scratchSize / pixelsPerChar;
    if(maxScratchChars == 0)
    {
        // Not enough memory to fit even one char
        return 0;
    }
    const unsigned maxScratchWidth = maxScratchChars * font->width;

    unsigned lineWidth = 0, lineHeight = font->height; // Width/height of scratch buffer rect for this line

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
        const unsigned nextLineY = *y + font->height;

        if(*y >= self->height)
        {
            // Line outside screen - exit
            return 1;
        }
        else if(nextLineY >= self->height)
        {
            // Line only partially visible - clip
            lineHeight = font->height - (nextLineY - self->height);
        }
#endif

        unsigned charX = *x;    // Because `*x` is only updated once per `writeRect()`
        unsigned charRowStride; // bytes between consequent rows of a character bitmap in the scratch buffer

        iCh = lineStart;
        for(unsigned i = 0; i < charsThisLine; i += maxScratchChars)
        {
            unsigned charWidth = font->width;
            WGFX_U8 *charBuffer = self->scratchData;
            unsigned charBufferWidth = 0;
            charRowStride = (maxScratchWidth - font->width) * self->bpp;

            for(unsigned j = 0; j < maxScratchChars; j++)
            {
                const unsigned nextCharX = charX + font->width;

#ifndef WGFX_NO_CLIPPING
                if(nextCharX >= self->width)
                {
                    // Current char only partially visible - clip it
                    charWidth = font->width - (nextCharX - self->width);
                    charRowStride = (maxScratchWidth - charWidth) * self->bpp;
                }
                else if(charX >= self->width)
                {
                    // Current char out of screen bounds (to the right)
                    break;
                }
#endif
                writeMonoChar(*iCh, charBuffer, self->bpp, charRowStride, font, charWidth, lineHeight, fgColor, bgColor);
                charBuffer += charWidth * self->bpp; // Go right to the top-left corner of next char
                iCh++;
                charBufferWidth += charWidth;
                charX += charWidth;
            }

            // Draw the whole scratch buffer to the screen when:
            // - Scratch buffer full
            // - '\n' reached
            // - End of string reached
            self->writeRect(*x, *y, charBufferWidth, lineHeight, self->scratchData, self->userPtr);
            *x += charBufferWidth;

            if(charX >= self->width)
            {
                if(wrapMode & WGFX_WRAP_RIGHT)
                {
                    // Wrap and continue from `iCh`
                    *x = startX;
                    *y += lineHeight;
                }
                else
                {
                    // Wait for next line
                    break;
                }
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
        iCh++; // Skip the '\n' or end char
    }

    return 1;
}

void wgfxDrawBitmap(WGFXscreen *self, const WGFX_U8 *image, unsigned imgW, unsigned imgH,
                    unsigned x, unsigned y, unsigned w, unsigned h)
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

    if(w == imgW && h == imgH)
    {
        self->writeRect(x, y, w, h, image, self->userPtr);
    }
    else
    {
        // Need to draw the image in chunks...
        WGFX_U8 *scratchBuf = self->scratchData;
        const unsigned nScratchRows = self->scratchSize / w;
        const unsigned bytesPerRow = w * self->bpp, imageRowStride = imgW * self->bpp;

        for(unsigned row = 0; row < h; row += nScratchRows)
        {
            const unsigned nBufRows = MIN(nScratchRows, h - row);
            for(unsigned iBufRow = 0; iBufRow < nBufRows; iBufRow++)
            {
                WGFX_MEMCPY(scratchBuf, image, bytesPerRow);
                image += imageRowStride;
                scratchBuf += bytesPerRow;
            }

            self->writeRect(x, y, w, nBufRows, self->scratchData, self->userPtr);
            y += nBufRows;
        }
    }
}