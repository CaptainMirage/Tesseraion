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

TBD.
