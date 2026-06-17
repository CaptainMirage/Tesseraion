// glyphs.h -- the ASCII glyph atlas.
//
// The ramp characters are rasterized side by side into one single-channel (R8)
// GL texture, one fixed-size tile per glyph in ramp order. The fragment shader
// picks a tile by field intensity and samples that tile's coverage to draw the
// glyph. Host-agnostic: assumes a current GLES 3.0 context.

#ifndef TESS_GLYPHS_H
#define TESS_GLYPHS_H

#include <GLES3/gl3.h>
#include <stdbool.h>

/// A built glyph atlas. Tiles are laid out left to right in ramp order, each
/// `tile_w` x `tile_h` texels, so glyph i spans u in [i/count, (i+1)/count].
typedef struct {
    GLuint texture;   ///< R8 atlas texture; 0 until built.
    int    count;     ///< glyph tiles (== ramp length).
    int    tile_w;    ///< tile width in atlas texels.
    int    tile_h;    ///< tile height in atlas texels.
} tess_glyphs;

/// Rasterize `ramp` into the atlas from the embedded monospace font, sized for
/// the actual screen cell (cell_w_px x cell_h_px) so on-screen scaling stays
/// near 1:1 and the glyphs stay crisp. The glyph is drawn at font_px =
/// cell_h_px / line_h_ratio and top-aligned within the tile, matching the web
/// canvas layout. `supersample` (>=1) bakes the tiles that many times larger for
/// clean downsampling. Requires a current GLES context. Returns true on success
/// (logs and zeroes on failure).
bool tess_glyphs_build(tess_glyphs *g, const char *ramp,
                       float cell_w_px, float cell_h_px, float line_h_ratio,
                       int supersample);

/// Delete the texture and zero the struct. Safe on a zeroed struct.
void tess_glyphs_destroy(tess_glyphs *g);

#endif // TESS_GLYPHS_H
