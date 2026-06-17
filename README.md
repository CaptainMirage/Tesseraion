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

Requirements: a C11 compiler, `make`, `pkg-config`, and the GLFW3 and OpenGL ES
development headers. On Arch: `pacman -S base-devel glfw mesa`.

```bash
make          # release build (-O2 -Wall -Wextra -Werror), produces ./tesseraion
make run      # build, then launch the GLFW dev host
make debug    # unoptimized build with symbols (-O0 -g)
make clean
```

The renderer targets OpenGL ES 3.0 / GLSL ES 3.00 (the same feature set as WebGL2,
broadly supported by Mesa). It runs from the repo root so it can find `shaders/`.

## Run

```bash
./tesseraion [path/to/config]    # default config path is ./tesseraion.conf
```

Keys in the dev host:

- `Esc` quit
- `B`   toggle smooth glyph cross-fading (on) vs one hard glyph per cell (off)

The shader files and the config file are watched while running, so edits apply
live (see below).

## Configuration

All tunables live in a small `key = value` file. Start from the documented sample:

```bash
cp tesseraion.conf.example tesseraion.conf
```

Then uncomment and edit keys; save and the running app re-reads the file
instantly (the frame cap and everything else update live). `#` starts a comment,
unknown keys are warned and ignored. Keys include the frame cap and `speed`; the
cell metrics (`font_size`, `char_w_ratio`, `line_h_ratio`, `max_cols`); the
`ramp`; the noise field (`noise_scale`, `warp`, `softness`, `skip`, `alpha_cap`,
`fade_band`); the palette (`mid_rgb`, `peak_rgb`, `blue_start`, `blue_full`,
`accent_boost`); and `seed`. See `tesseraion.conf.example` for the full list with
defaults and descriptions.

The pattern is randomized on each launch unless you pin it with `seed = <number>`.
Editing `speed` changes the drift rate without jumping the pattern. Your local
`tesseraion.conf` is gitignored, so tweaking it never dirties the repo.

## Architecture

The code is split into a host-agnostic render core and a swappable host, so the
visual can be reused unchanged by different windowing backends:

- `src/core/` is the render core: it assumes a current GLES 3.0 context and
  exposes a tiny C API (`init`, `resize`, `draw(time)`, `shutdown`) plus a config
  apply / hot-reload surface. It owns the shader, the glyph atlas, and all GL
  state; it never touches windowing, input, or timing.
- `src/host/` owns the window, the context, the clock, and input, and drives the
  core through that API. `host_glfw.c` is the windowed dev host.
- `shaders/ascii.frag` is organised as three editable stages (pattern, glyph,
  palette) and documents the full uniform contract at the top of the file.

This split is deliberate: a layer-shell wallpaper host (COSMIC, Hyprland, sway,
river) can be added later as a new host that only supplies its own GLES context,
reusing the render core untouched. The exact handoff is documented in
`src/host/host.h`.

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
