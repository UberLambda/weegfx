# weegfx
**A simple C99 graphics library for embedded systems, where the whole framebuffer won't fit in SRAM.**

The library is split into:
- A [software-rendering core](src/), that is as platform-agnostic as possible (it theoretically only requires a working C99 compiler)
- Hardware-specific [`backends/`](backends/) - see each backend's API for more information.

## Usage
The user allocates and fills a `WGFXscreen` instance for each screen surface they want to control.  
The instance asks for a pointer to a "scratch buffer" (i.e., a partial framebuffer). weegfx renders everything to this scratch buffer, filling the biggest amount of pixels it can in it; each rendering operation then consists of:
- A call to the user-(or backend-)provided `beginWrite()` function, to select the area of the screen surface to draw on.
- As many calls to the user-(or backend-)provided `write()` function as needed to fill the selected area with pixel data from the scratch buffer.
- A call to the user-(or backend-)provided `endWrite()`, to end the drawing/data transfer operation.  
This model is very well-suited for external display controllers, that usually work in the same way (begin write operation, select screen area ➡️ fill screen area, likely via DMA ➡️ flush writes, end write operation).

Refer to [`weegfx.h`](src/weegfx.h) for further information, and choose a backend from [`backends/`](backends) if you need more than just software rendering. See each backend's main header for documentation.

## Arduino
weegfx can be used as an Arduino library (see [`library.properties`](library.properties)) if needed. Clone weegfx to your `Arduino/libraries/`, and copy any relevant backend from [`backends/`](backends) to your sketch's `src/` folder if you need one.

## Fonts
[`tools/fontconv.py`](tools/fontconv.py) can be used to generate weegfx bitmap font headers from font files (builtin .bdf support; .ttf/.odf/other formats read by FreeType).

## License
Copyright (c) 2019-2020 Paolo Jovon \<paolo.jovon@gmail.com\>  
Released under the terms of the [BSD 3-clause license](LICENSE).
