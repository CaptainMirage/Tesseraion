# Tesseraion - GNOME wallpaper host

Runs the Tesseraion animated ASCII-noise renderer as your GNOME desktop
background, on Wayland.

GNOME's compositor (Mutter) has no layer-shell, so this ships as a GNOME Shell
extension paired with a small native host. The host is a normal Wayland client
that brings up an OpenGL ES 3.0 surface and drives the shared render core; the
extension spawns it as a shell-owned client and clones its surface into each
monitor's background actor, below your windows and hidden from the overview,
alt-tab, and the window list. The render core itself is reused unchanged (see the
repository root README).

## Requirements

- GNOME Shell 50 on a Wayland session.
- To build: a C11 compiler, `make`, `wayland-scanner` + `wayland-protocols`, and
  the dev libraries for `wayland-client`, `wayland-egl`, `egl`, and `glesv2`.

## Quick start

```bash
cd gnome
make                # build the host (tesseraion-gnome)
make install        # stage the extension into your GNOME extensions dir
```

Then log out and back in once (a running Wayland shell does not pick up a
brand-new extension folder), and enable it:

```bash
gnome-extensions enable tesseraion@captainmirage.github.io
```

To turn it off / remove it:

```bash
gnome-extensions disable tesseraion@captainmirage.github.io
make uninstall
```

## Documentation

- [docs/BUILD.md](docs/BUILD.md) - building and installing in detail, and the
  `make` targets.
- [docs/CONFIGURATION.md](docs/CONFIGURATION.md) - the look/behavior knobs and live
  reload.
- [docs/TESTING.md](docs/TESTING.md) - try it safely in a throwaway account without
  touching your real desktop.
- [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) - blur-my-shell, multi-monitor,
  HiDPI, and common issues.

## Status and license

Beta (`gnome-0.2.x`); see [../VERSIONING.md](../VERSIONING.md). Licensed
GPL-3.0-or-later, like the rest of the project.
