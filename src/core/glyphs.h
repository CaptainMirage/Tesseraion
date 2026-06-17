// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
// glyphs.h -- the ASCII glyph atlas.
//
// The ramp characters are rasterized into a single-channel (R8) 2D array
// texture, one layer per glyph in ramp order, with mipmaps. Per-glyph layers
// keep mipmapped minification from bleeding between neighbours, and let the
// shader cross-fade adjacent ramp glyphs by sampling two layers. Host-agnostic:
// assumes a current GLES 3.0 context.

#ifndef TESS_GLYPHS_H
#define TESS_GLYPHS_H

#include <GLES3/gl3.h>
#include <stdbool.h>

/// A built glyph atlas: a 2D array texture with one tile_w x tile_h layer per
/// glyph, layer i == ramp[i]. Mipmapped.
typedef struct {
    GLuint texture;   ///< R8 GL_TEXTURE_2D_ARRAY; 0 until built.
    int    count;     ///< layers (== ramp length).
    int    tile_w;    ///< layer width in texels.
    int    tile_h;    ///< layer height in texels.
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
