// weegfx - A small graphics library for embedded systems with low RAM.
// Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
// Released under the 3-clause BSD license (see LICENSE)
#include "weegfx.h"

#include "weegfx/base.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void wgfxFillRect(WGFXscreen *self, unsigned x, unsigned y, unsigned w, unsigned h, const void *color)
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
        const unsigned fillH = MIN(h, self->scratchSize);
        const unsigned fillW = MIN(self->scratchSize / fillH, w);

        for(unsigned iy = y; iy < endY; iy += MIN(fillH, endY - iy))
        {
            for(unsigned ix = x; ix < endX; ix += MIN(fillW, endX - ix))
            {
                self->writeRect(ix, iy, fillW, fillH, self->scratchData, self->userPtr);
            }
        }
    }
}