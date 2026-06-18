# Building and installing

The GNOME host lives entirely under `gnome/` and compiles the shared render core
from `../src/core` without modifying it. Its own build is independent of the
repository's root Makefile.

## Dependencies

- A C11 compiler and `make`.
- `wayland-scanner` and `wayland-protocols` (the xdg-shell protocol stubs are
  generated at build time).
- Development libraries: `wayland-client`, `wayland-egl`, `egl`, `glesv2`.

On Arch / EndeavourOS these come from `wayland`, `wayland-protocols`, `mesa`, and
the base toolchain (`base-devel`).

## Build

```bash
cd gnome
make
```

This generates the xdg-shell client stubs, compiles the core sources plus the
host, and links `tesseraion-gnome`. The build is `-O2 -Wall -Wextra -Werror`
clean.

Other targets:

| Target          | What it does                                                       |
| --------------- | ------------------------------------------------------------------ |
| `make`          | Build the host binary.                                             |
| `make run`      | Run the host directly as a normal window (handy for a quick look). |
| `make debug`    | Build at `-O0 -g` (still warnings-as-errors).                      |
| `make install`  | Stage the extension into your GNOME extensions directory.          |
| `make uninstall`| Remove the staged extension directory.                             |
| `make enable`   | Install, then enable the extension.                                |
| `make disable`  | Disable the extension.                                             |
| `make logs`     | Follow the extension's and host's journal output.                  |
| `make clean`    | Remove build artifacts, generated protocol, and the binary.        |

## What `make install` stages

The extension is self-contained: `make install` copies the JS and `metadata.json`,
the `tesseraion-gnome` binary, the `shaders/` the core loads at runtime, and a
default `tesseraion.conf` (from the repo's `tesseraion.conf.example`) into:

```
~/.local/share/gnome-shell/extensions/tesseraion@captainmirage.github.io/
```

The host is launched with its working directory set to that folder, so the
core's shader and config paths resolve next to the binary.

## Enabling

A running Wayland GNOME Shell does not rescan the extensions directory for a
newly added folder, so after the first `make install` you must log out and back
in once. After that, the extension is registered and you can toggle it freely:

```bash
gnome-extensions enable  tesseraion@captainmirage.github.io
gnome-extensions disable tesseraion@captainmirage.github.io
```

Enabling and disabling take effect immediately (no further re-login): the
extension installs its hooks and spawns the host on enable, and cleanly restores
everything and stops the host on disable.

To try it without touching your real desktop first, see [TESTING.md](TESTING.md).
