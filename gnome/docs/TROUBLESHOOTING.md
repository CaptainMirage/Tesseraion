# Troubleshooting

First stop for anything: watch the logs while you enable/disable.

```bash
make logs
# or:
journalctl --user -f -o cat | grep -iE "tesseraion|captainmirage"
```

## The extension does not appear in the Extensions app

A running Wayland GNOME Shell does not pick up a newly installed extension folder.
Log out and back in once after the first `make install`; it will be registered
after that, and enable/disable then work without further re-logins.

## The wallpaper is frozen, not animating

The host advances only when the compositor presents its surface. If the field is
static:

- Make sure something is actually showing it (the extension is enabled and the
  clone is visible, i.e. you are not locked or fully covered by a fullscreen
  window, which intentionally pause it).
- Check `make logs` for a host crash or a GL/EGL error.

Animating only while the mouse moves was an early bug (the host now drives itself
off frame callbacks) and should not occur.

## blur-my-shell still shows the old wallpaper

The extension nudges blur-my-shell to re-capture when it (de)attaches the clone,
so the blur should follow the live field. If it ever goes stale, toggle this
extension off and on, or toggle blur-my-shell, to force a fresh snapshot.

## Multiple monitors

A single host is rendered and cloned onto every monitor, sized to the largest
connected output and scaled to fit the others. Plugging or unplugging a display,
or changing resolution, re-sizes the host automatically. All monitors show the
same field (independent per-monitor fields are not implemented yet).

## HiDPI / fractional scaling looks soft

The host renders at the monitor's logical size, which is exact at 100% scale. On a
fractionally scaled display the result is upscaled by the compositor and the glyphs
can look slightly soft. Crisp physical-pixel rendering under fractional scaling is
a planned addition. As a workaround, a lower `glyph_filter` setting (see
[CONFIGURATION.md](CONFIGURATION.md)) or 100% scaling keeps it sharp.

## A stray Tesseraion window in alt-tab / overview

The host window is meant to stay hidden. If a leftover ever shows (for example
after a crash mid-enable), disabling the extension and re-enabling it reclaims and
hides the window. If the host process was orphaned, `pkill tesseraion-gnome` and
re-enable.

## Removing it completely

```bash
gnome-extensions disable tesseraion@captainmirage.github.io
make uninstall
```
