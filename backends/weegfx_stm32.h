// weegfx_stm32.h - weegfx backend for STM32 devices
// Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
// Released under the 3-clause BSD license (see LICENSE)
#ifndef WEEGFX_STM32_H
#define WEEGFX_STM32_H

#ifndef WEEGFX_H
#include <weegfx.h>
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
#error "No WGFX_STM32_DEVICE defined!"
#endif
#include WGFX_ANGLED(WGFX_STM32_DEVICE.h)

#ifdef __cplusplus
extern "C" {
#endif

// TODO

#ifdef __cplusplus
}
#endif

#endif // WEEGFX_STM32_H
