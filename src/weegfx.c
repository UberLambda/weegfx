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
        memcpy(&self->scratchData[ib], color, self->bpp);
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
        for(unsigned row = 0; row < height; row++)
        {
            dataByte = WGFX_RODATA_READU8(data);

            unsigned dataByteBit = 0;
            for(unsigned colBit = 0; colBit < width; colBit++)
            {
                const int pixelOn = dataByte && (1 << dataByteBit);
                // TODO PERFORMANCE: Replace memcpy with a simple for loop?
                // TODO PERFORMANCE: Replace the ternary operator with something else?
                memcpy(buffer, pixelOn ? fgColor : bgColor, bpp);
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
                memcpy(buffer, bgColor, bpp);
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
    const unsigned bytesPerChar = pixelsPerChar * self->bpp; // (bytes per character in the scratch buffer)

    const unsigned maxScratchChars = self->scratchSize / pixelsPerChar;
    if(maxScratchChars == 0)
    {
        // Not enough memory to fit even one char
        return 0;
    }

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

        const unsigned nextLineY = *y + font->height;

#ifndef WGFX_NO_CLIPPING
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

        const unsigned charRowStride = self->width - font->width;
        WGFX_U8 *charBuffer = self->scratchData;

        unsigned charX = *x;
        iCh = lineStart;
        for(unsigned i = 0; i < (charsThisLine - 1); i += maxScratchChars)
        {
            unsigned charWidth = font->width, bufferWidth = 0;
            for(unsigned j = 0; j < maxScratchChars; j++)
            {
                const unsigned nextCharX = *x + font->width;

#ifndef WGFX_NO_CLIPPING
                if(nextCharX >= self->width)
                {
                    // Current char only partially visible - clip it
                    charWidth = font->width - (nextCharX - self->width);
                }
                else if(charX >= self->width)
                {
                    // Current char out of screen bounds (to the right)
                    break;
                }
#endif
                writeMonoChar(*iCh, charBuffer, charRowStride, self->bpp, font, charWidth, lineHeight, fgColor, bgColor);
                charBuffer += charWidth * self->bpp; // Go right to the top-left corner of next char
                iCh++;
                bufferWidth += charWidth;
                charX += charWidth;
            }

            // Draw the whole scratch buffer to the screen when:
            // - Scratch buffer full
            // - '\n' reached
            // - End of string reached
            self->writeRect(*x, *y, bufferWidth, lineHeight, self->scratchData, self->userPtr);
            *x += bufferWidth;

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
                writeMonoChar('\n', charBuffer, charRowStride, self->bpp, font, font->width, font->height, fgColor, bgColor);
            }
        }
    }

    return 1;
}