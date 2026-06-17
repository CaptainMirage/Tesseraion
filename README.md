# Tesseraion

Animated Perlin-noise ASCII-art wallpaper renderer, written in C and GLSL ES.
Lean, dependency-light, and built to be portable across layer-shell desktops
(COSMIC, Hyprland, sway, river) and anywhere GLES runs. Work in progress.

## The name

Tesseraion is a coined word, a fusion of two old ones:

- Tessera (Latin, from the Greek "tessares", meaning four): a single small tile
  in a mosaic, originally a four-sided piece. A finished mosaic is a field of
  tesserae. This program draws exactly that, a grid of ASCII cells, each one a
  tile in the larger picture.
- Aion (Greek, "aion"): an age, an eternity, time as something cyclical and
  unbounded. The field never stops. It flows on a continuous clock with no end
  and no loop you can quite catch.

Put together, Tesseraion means a mosaic of tiles in endless motion. Tiles out of
time. It is also, conveniently, unique

## What it does

A fullscreen field of ASCII characters driven by animated Perlin noise. The noise
value at each cell picks a glyph and a shade, and the whole field drifts over
time. The look is ported from the animated background of my portfolio site.

## Build

Build instructions land here as the project takes shape.

## License

This project is licensed under the **GNU General Public License v3.0 or later**
(GPL-3.0-or-later) - see the [LICENSE](LICENSE) file for the full text. Every
source file carries an `SPDX-License-Identifier: GPL-3.0-or-later` header.

In plain terms: **the code is mine (Captain Mirage)** and it is copyleft. You're
free to use, study, modify, and share it, but any version you distribute must
**also be open source under the GPL**, **keep the copyright and license notices**,
and **state the changes** you made. If you build on it, **mention me and link
back to this repository.** Using it without attribution is not cool, and will
make me angry.

The bundled font (`third_party/font_vera_mono.h`) is Bitstream Vera Sans Mono
under its own permissive license (see `third_party/VeraMono-LICENSE.txt`), and
`third_party/stb_truetype.h` is public domain / MIT; both are compatible with the
GPL.

(c) 2026 Captain Mirage
