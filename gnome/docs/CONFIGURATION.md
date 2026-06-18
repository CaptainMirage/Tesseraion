# Configuration

The wallpaper reads `tesseraion.conf` from the install directory:

```
~/.local/share/gnome-shell/extensions/tesseraion@captainmirage.github.io/tesseraion.conf
```

`make install` puts a default there (a copy of the repo's
`tesseraion.conf.example`). Edit that file and save; the running host re-reads it
and applies the change live, no re-enable or re-login needed. The same goes for
the shader files in the install's `shaders/` folder.

The format is one `key = value` per line, `#` starts a comment. Any key you leave
out keeps its built-in default. The authoritative, commented list is
[../../tesseraion.conf.example](../../tesseraion.conf.example); the groups are:

| Group              | Keys                                                            | Controls                                              |
| ------------------ | --------------------------------------------------------------- | ----------------------------------------------------- |
| Host / timing      | `fps_cap`, `speed`                                              | frame cap (GPU load) and how fast the field drifts.   |
| Cell metrics       | `font_size`, `char_w_ratio`, `line_h_ratio`, `max_cols`        | the glyph cell size and grid density.                 |
| Glyph quality      | `supersample`, `glyph_filter`                                  | crispness of the rasterized glyphs.                   |
| Char ramp          | `ramp`                                                          | the sparse-to-dense glyph set.                        |
| Noise field        | `noise_scale`, `warp`, `softness`, `skip`, `alpha_cap`, `fade_band` | the look and motion of the field.                |
| Seed               | `seed`                                                         | pin for a fixed pattern, omit for a random one.       |
| Palette            | `mid_rgb`, `peak_rgb`, `blue_start`, `blue_full`, `accent_boost` | the gray-to-blue colouring.                         |

## Notes for the wallpaper

- `fps_cap` is the main knob for how much GPU an idle wallpaper uses. Lower it
  (e.g. 30) for a calmer, lighter background.
- The host also idles on its own whenever it cannot be seen (session locked, a
  fullscreen window covering the monitor, or the display asleep), so the cap is
  the steady-state ceiling, not a constant draw.
- Smaller `font_size` (or a larger `max_cols`) gives a finer, denser field.
