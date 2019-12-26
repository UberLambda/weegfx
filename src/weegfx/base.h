// weegfx/base.h - weegfx base (system) functions and typedefs
// Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
// Released under the 3-clause BSD license (see LICENSE)
#ifndef WEEGFX_BASE_H
#define WEEGFX_BASE_H

#ifndef WGFX_NO_STRING
#    include <string.h>
#    ifndef WGFX_MEMCPY
#        define WGFX_MEMCPY memcpy
#    endif
#    ifndef WGFX_MEMMOVE
#        define WGFX_MEMMOVE memmove
#    endif
#    ifndef WGFX_MEMSET
#        define WGFX_MEMSET memset
#    endif
#endif

#ifndef WGFX_FORCEINLINE
#    if defined(__GNUC__) || defined(__clang__)
#        define WGFX_FORCEINLINE inline __attribute__((always_inline))
#    else
#        define WGFX_FORCEINLINE inline
#    endif
#endif

// `WGFX_RODATA`: Puts a `static const` variable in readonly memory.
// `WGFX_RODATA_READU8(addr)`: Reads a byte from an address (pointer) into a `WGFX_RODATA` variable.
// - On AVR: puts data is PROGMEM and accessed through <avr/pgmspace.h>
// - Other platforms with a GCC-like compiler: data is put in the `.rodata` section
// - Anything else: no-op
// #define WGFX_RODATA and WGFX_RODATA_READU8(addr) for custom behaviour.
// `WGFX_RODATA_MEMCPY(dest, src, size)`: Like `memcpy()`, but `src` is assumed to be in RODATA.
#ifndef WGFX_RODATA
#    ifdef __AVR__
#        include <avr/pgmspace.h>
#        define WGFX_RODATA PROGMEM
#    else
#        if defined(__GNUC__) || defined(__clang__)
#            define WGFX_RODATA __attribute__((aligned(4), section(".rodata")))
#        else
#            define WGFX_RODATA
#        endif
#    endif
#endif
#ifndef WGFX_RODATA_READU8
#    ifdef __AVR__
#        include <avr/pgmspace.h>
#        define WGFX_RODATA_READU8(addr) pgm_read_byte((addr))
#    else
#        define WGFX_RODATA_READU8(addr) (*(addr))
#    endif
#endif
#ifndef WGFX_RODATA_MEMCPY
#    ifdef __AVR__
#        include <avr/pgmspace.h>
#        define WGFX_RODATA_MEMCPY memcpy_P
#    else
#        define WGFX_RODATA_MEMCPY WGFX_MEMCPY
#    endif
#endif

#endif // WEEGFX_BASE_H