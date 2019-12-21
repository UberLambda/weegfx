// weegfx/types.h - weegfx core typedefs
// Copyright (c) 2019 Paolo Jovon <paolo.jovon@gmail.com>
// Released under the 3-clause BSD license (see LICENSE)
#ifndef WEEGFX_TYPES_H
#define WEEGFX_TYPES_H

#ifndef WGFX_NO_STDINT
#    include <stdint.h>
#    ifndef WGFX_U8
#        define WGFX_U8 uint8_t
#    endif
#    ifndef WGFX_U16
#        define WGFX_U16 uint16_t
#    endif
#    ifndef WGFX_U32
#        define WGFX_U32 uint32_t
#    endif
#else
#    ifndef WGFX_U8
#        define WGFX_U8 unsigned char
#    endif
#    ifndef WGFX_U16
#        define WGFX_U16 unsigned short
#    endif
#    ifndef WGFX_U32
#        define WGFX_U32 unsigned long
#    endif
#endif

#ifndef WGFX_NO_STDDEF
#    include <stddef.h>
#    ifndef WGFX_SIZET
#        define WGFX_SIZET size_t
#    endif
#else
#    ifndef WGFX_SIZET
#        define WGFX_SIZET unsigned long
#    endif
#endif

#endif // WEEGFX_TYPES_H