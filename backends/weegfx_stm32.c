// weegfx_stm32.c - weegfx backend for STM32 devices
// Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
// Released under the 3-clause BSD license (see LICENSE)
#include "weegfx_stm32.h"

// -- "Inspired" by https://vivonomicon.com/2019/07/05/bare-metal-stm32-programming-part-9-dma-megamix/ --

/// Maximum number of bytes that can be sent at once over DMA.
#define DMA_MAX_TRANSFER_SIZE 0xFFFF

int wgfxSTM32Init(WGFXstm32Backend *self, unsigned priority)
{
    if(!(self && self->spi && self->dma && self->dmaChannel && self->beginScreenWrite && self->endScreenWrite))
    {
        return 0;
    }
    if(priority > 0x3)
    {
        priority = 0x3;
    }

    // 8/16-bit memory size, 8/16-bit peripheral size, increment memory pointer but not peripheral, memory -> peripheral
    const unsigned dmaSize = (self->spi->CR1 & SPI_CR1_DFF) ? 0x1 : 0x0; // Either 16 or 8 bits
    self->dmaChannel->CCR = 0x00000000;
    self->dmaChannel->CCR |= (priority << DMA_CCR_PL_Pos) | (dmaSize << DMA_CCR_MSIZE_Pos) | (dmaSize << DMA_CCR_PSIZE_Pos) | DMA_CCR_MINC | DMA_CCR_DIR;

    // Destination = the SPI data register
    self->dmaChannel->CPAR = (WGFX_U32)&self->spi->DR;

    // Clear pending transfer complete/error bits
    self->dma->IFCR |= self->dmaISRDoneMask;

    return 1;
}

/// Setup the DMA channel to transfer a buffer to SPI and enable it.
WGFX_FORCEINLINE void dmaSpiTx(WGFXstm32Backend *self, const void *buf, WGFX_SIZET size)
{
    self->dmaChannel->CNDTR = size;
    self->dmaChannel->CMAR = (WGFX_U32)buf;
    // Start the DMA channel
    self->dmaChannel->CCR |= DMA_CCR_EN;
}

/// Spinlock waiting for a DMA transfer to complete / error out, then clear ISR flags and disable the DMA channel.
WGFX_FORCEINLINE void dmaWait(WGFXstm32Backend *self)
{
    // Until the DMA channel is disabled or a transfer complete event / transfer error event happens...
    while((self->dmaChannel->CCR & DMA_CCR_EN) && !(self->dma->ISR & self->dmaISRDoneMask)) {}
    // Clear interrupt flags
    self->dma->IFCR |= self->dmaISRGlobalMask;
    // Disable the DMA channel
    self->dmaChannel->CCR &= ~DMA_CCR_EN;
}

void wgfxSTM32Write(const WGFX_U8 *buf, WGFX_SIZET size, void *userPtr)
{
    WGFXstm32Backend *self = (WGFXstm32Backend *)userPtr;

    dmaWait(self);

    WGFX_SIZET i;
    for(i = DMA_MAX_TRANSFER_SIZE; i < size; i += DMA_MAX_TRANSFER_SIZE)
    {
        dmaSpiTx(self, buf, DMA_MAX_TRANSFER_SIZE);
        dmaWait(self);
    }
    if((size - (i - DMA_MAX_TRANSFER_SIZE)) > 0)
    {
        dmaSpiTx(self, buf, size - i);
        dmaWait(self);
    }
}

void wgfxSTM32BeginWrite(unsigned x, unsigned y, unsigned w, unsigned h, void *userPtr)
{
    WGFXstm32Backend *self = (WGFXstm32Backend *)userPtr;
    if(self->beginScreenWrite)
    {
        self->beginScreenWrite(x, y, w, h, self->backendUserPtr);
    }
}

void wgfxSTM32EndWrite(void *userPtr)
{
    WGFXstm32Backend *self = (WGFXstm32Backend *)userPtr;
    if(self->endScreenWrite)
    {
        self->endScreenWrite(self->backendUserPtr);
    }
}