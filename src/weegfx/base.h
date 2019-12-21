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

#endif // WEEGFX_BASE_H