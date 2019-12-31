// weegfx_stm32.h - weegfx backend for STM32 devices
// Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
// Released under the 3-clause BSD license (see LICENSE)
#ifndef WEEGFX_STM32_H
#define WEEGFX_STM32_H

#ifndef WEEGFX_H
#    include <weegfx.h>
#endif

#define WGFX_ANGLED(name) <name>

#ifndef WGFX_STM32_DEVICE
// WGFX_STM32_DEVICE: The name of the STM32CubeMX-generated header for the device, excluding the .h extension.
// For example, to target a STM32F103C8:
// ```
// #define STM32F103xB
// #define WGFX_STM32_DEVICE stm32f1xx
// ```
// before including this header.
// Note that header filenames are case-sensitive on Linux!
#    error "No WGFX_STM32_DEVICE defined!"
#endif
#include WGFX_ANGLED(WGFX_STM32_DEVICE.h)

#ifdef __cplusplus
extern "C" {
#endif

/// Called before writing pixel data to the screen.
typedef void (*WGFXstm32BeginScreenWritePFN)(unsigned x, unsigned y, unsigned w, unsigned h, void *backendUserPtr);

/// Called after the pixel data is written to the screen.
typedef void (*WGFXstm32EndScreenWritePFN)(void *backendUserPtr);

/// An instance of the STM32 backend for weegfx.
typedef struct
{
    /// The bytes per pixel to transfer.
    unsigned bpp;

    /// The SPI to use for sending pixel data.
    SPI_TypeDef *spi;

    /// The DMA used to copy pixel data from memory to SPI.
    DMA_TypeDef *dma;

    /// The DMA channel in `dma` used to copy pixel data from memory to SPI.
    DMA_Channel_TypeDef *dmaChannel;

    /// `(1 << DMA_ISR_TCIFx_Pos) | (1 << DMA_ISR_TEIFx)` for x = index of `dmaChannel`;
    /// ORed with `dma`'s `ISR` to know when a write is complete.
    WGFX_U32 dmaISRDoneMask;

    /// `DMA_ISR_TCIFx_GIF` for x = index of `dmaChannel`;
    /// ORed with `dma`'s `ICFR` to clear DMA transfer flags.
    WGFX_U32 dmaISRGlobalMask;

    /// Called before pixel data for the given rect is written to the screen via SPI. Can be null.
    ///
    /// Use this to assert the CS pin, set the address window and prepare the screen for receiving data.
    WGFXstm32BeginScreenWritePFN beginScreenWrite;

    /// Called after pixel data has been written to the screen via SPI. Can be null.
    ///
    /// Use this to deassert the CS pin.
    WGFXstm32EndScreenWritePFN endScreenWrite;

    /// User pointer, passed as-is to `beginScreenWrite` and `endScreenWrite`.
    void *backendUserPtr;
} WGFXstm32Backend;

/// Initializes the DMA channel at `self->dma` so that it will transfer data to
/// `self->spi` (this means that `self` should already have been populated by the user!)
/// `priority` is the priority to set for the DMA channel (0 to 3).
/// Returns false on error.
///
/// Note that this does NOT enable the DMA, or try to change the configuration of the SPI!
/// (don't forget to enable the peripherals, set `TXDMAEN` in the SPI's `CR2` and/or DMA remap bits in `SYSCFG`!)
int wgfxSTM32Init(WGFXstm32Backend *self, unsigned priority);

/// The `beginWrite` implementation for STM32.
/// Set the `userPtr` of the `WGFXscreen` to point to a valid `WGFXstm32Backend`!
void wgfxSTM32BeginWrite(unsigned x, unsigned y, unsigned w, unsigned h, void *userPtr);

/// The `write` implementation for STM32.
/// Set the `userPtr` of the `WGFXscreen` to point to a valid `WGFXstm32Backend`!
void wgfxSTM32Write(const WGFX_U8 *buf, WGFX_SIZET size, void *userPtr);

/// The `endWrite` implementation for STM32.
/// Set the `userPtr` of the `WGFXscreen` to point to a valid `WGFXstm32Backend`!
void wgfxSTM32EndWrite(void *userPtr);

#ifdef __cplusplus
}
#endif

#endif // WEEGFX_STM32_H
