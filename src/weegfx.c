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

    const WGFX_SIZET xferCount = w * h;
    const WGFX_SIZET scratchSizeB = self->scratchSize * self->bpp;
    const int rectFitsScratch = self->scratchSize >= xferCount;

    const WGFX_SIZET fillCount = rectFitsScratch ? fillCount : self->scratchSize;
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

            for(unsigned colBit = 0; colBit < font->width; colBit++)
            {
                // Draw only up to `width`, but seek in the memory buffer up to `font->width`!
                if(colBit < width)
                {
                    const int pixelOn = dataByte & 0x80;
                    dataByte <<= 1;
                    // TODO PERFORMANCE: Replace memcpy with a simple for loop?
                    // TODO PERFORMANCE: Replace the ternary operator with something else?
                    WGFX_MEMCPY(buffer, pixelOn ? fgColor : bgColor, bpp);
                    buffer += bpp;
                }

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

        iCh = lineStart;

        const unsigned nChunks = (charsThisLine - 1) / maxScratchChars + 1;
        for(unsigned i = 0; i < nChunks; i++)
        {
            WGFX_U8 *chunkBuffer = self->scratchData;
            const unsigned nCharsThisChunk = MIN(maxScratchChars, charsThisLine - (iCh - lineStart));
            const unsigned maxChunkWidth = nCharsThisChunk * font->width;     // Hypothetical maximum width for this chunk
            const unsigned chunkWidth = MIN(maxChunkWidth, self->width - *x); // Actual width of this chunk

            // Render as many whole characters as possible
            unsigned xRight = 0;
            if(chunkWidth >= font->width)
            {
                const unsigned charRowStride = (chunkWidth - font->width) * self->bpp;
                const unsigned charStride = font->width * self->bpp; // Offset to go right to the top-left corner of next char

                for(xRight = font->width - 1; xRight < chunkWidth; xRight += font->width)
                {
                    writeMonoChar(*iCh, chunkBuffer, self->bpp, charRowStride, font, font->width, lineHeight, fgColor, bgColor);
                    chunkBuffer += charStride;
                    iCh++;
                }
                xRight -= (font->width - 1);
            }

            // Then render whatever is left (i.e. any last character that has to be clipped because it does not fit)
            const unsigned lastCharWidth = chunkWidth - xRight;
            if(lastCharWidth > 0)
            {
                const unsigned lastCharRowStride = (chunkWidth - lastCharWidth) * self->bpp;
                if(!(wrapMode & WGFX_WRAP_RIGHT))
                {
                    writeMonoChar(*iCh, chunkBuffer, self->bpp, lastCharRowStride, font, lastCharWidth, lineHeight, fgColor, bgColor);
                    iCh++;
                }
                else
                {
                    // Wrap right: instead of drawing a partially clipped char at the end of the line, the character will
                    // be drawn at the start of the next line.
                    // Still need to fill the rectangle with `bgColor` to prevent artefacts!
                    for(unsigned iRow = 0; iRow < lineHeight; iRow++)
                    {
                        for(unsigned iCol = 0; iCol < lastCharWidth; iCol++)
                        {
                            // TODO: Replace the memcpy with a simple for loop for performance?
                            WGFX_MEMCPY(chunkBuffer, bgColor, self->bpp);
                            chunkBuffer += self->bpp;
                        }
                        chunkBuffer += lastCharRowStride;
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

            if(chunkWidth < maxChunkWidth)
            {
                // Chunk got clipped on the right
                if(wrapMode & WGFX_WRAP_RIGHT)
                {
                    // Wrap and continue from `iCh`
                    *x = startX;
                    *y += lineHeight;
                }
                else
                {
                    // Discard all characters up to the end of this line
                    // Will continue from the start of next line...
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
