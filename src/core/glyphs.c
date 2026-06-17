// glyphs.c -- build the ASCII glyph atlas with stb_truetype (see glyphs.h).

#include "core/glyphs.h"

#include "stb_truetype.h"
#include "font_vera_mono.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool tess_glyphs_build(tess_glyphs *g, const char *ramp,
                       float cell_w_px, float cell_h_px, float line_h_ratio,
                       int supersample) {
    memset(g, 0, sizeof(*g));
    if (!ramp || !*ramp) {
        fprintf(stderr, "glyphs: empty ramp\n");
        return false;
    }
    if (supersample < 1) {
        supersample = 1;
    }
    if (line_h_ratio <= 0.0f) {
        line_h_ratio = 1.0f;
    }
    int count = (int)strlen(ramp);

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, tess_font_vera_mono,
                        stbtt_GetFontOffsetForIndex(tess_font_vera_mono, 0))) {
        fprintf(stderr, "glyphs: stbtt_InitFont failed\n");
        return false;
    }

    // The glyph is drawn at font_px within a line cell_h_px tall (the rest is
    // line spacing), matching the web canvas '<font_px>px monospace'.
    float font_px = cell_h_px / line_h_ratio;

    // Tiles match the screen cell, supersampled for clean downsampling.
    int tile_w = (int)lroundf(cell_w_px * (float)supersample);
    int tile_h = (int)lroundf(cell_h_px * (float)supersample);
    if (tile_w < 1) { tile_w = 1; }
    if (tile_h < 1) { tile_h = 1; }

    int atlas_w = tile_w * count;
    int atlas_h = tile_h;
    unsigned char *atlas = calloc((size_t)atlas_w * (size_t)atlas_h, 1);
    if (!atlas) {
        fprintf(stderr, "glyphs: out of memory for %dx%d atlas\n", atlas_w, atlas_h);
        return false;
    }

    // Map the em square to font_px (matches the canvas '<font_px>px monospace').
    // textBaseline 'top' puts the ascent at the tile top, so the baseline sits
    // `ascent` pixels down.
    float scale = stbtt_ScaleForMappingEmToPixels(&font, font_px * (float)supersample);
    int ascent = 0, descent = 0, linegap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &linegap);
    int baseline = (int)lroundf((float)ascent * scale);

    for (int ci = 0; ci < count; ci++) {
        int cp = (unsigned char)ramp[ci];
        if (cp == ' ') {
            continue;   // space stays a blank tile.
        }
        int gw = 0, gh = 0, gxoff = 0, gyoff = 0;
        unsigned char *bmp = stbtt_GetCodepointBitmap(&font, scale, scale, cp,
                                                      &gw, &gh, &gxoff, &gyoff);
        if (!bmp) {
            continue;   // missing glyph -> blank tile.
        }
        int tile_x0 = ci * tile_w;
        int dx0 = gxoff;             // left side bearing from the pen.
        int dy0 = baseline + gyoff;  // top of the glyph relative to the tile top.
        for (int y = 0; y < gh; y++) {
            int ty = dy0 + y;
            if (ty < 0 || ty >= tile_h) {
                continue;
            }
            for (int x = 0; x < gw; x++) {
                int tx = dx0 + x;
                if (tx < 0 || tx >= tile_w) {
                    continue;   // clip anything outside the tile.
                }
                atlas[(size_t)ty * (size_t)atlas_w + (size_t)(tile_x0 + tx)] =
                    bmp[y * gw + x];
            }
        }
        stbtt_FreeBitmap(bmp, NULL);
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);   // tightly packed single-channel rows.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlas_w, atlas_h, 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(atlas);

    if (glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "glyphs: GL error uploading atlas\n");
        glDeleteTextures(1, &tex);
        return false;
    }

    g->texture = tex;
    g->count   = count;
    g->tile_w  = tile_w;
    g->tile_h  = tile_h;
    return true;
}

void tess_glyphs_destroy(tess_glyphs *g) {
    if (g->texture) {
        glDeleteTextures(1, &g->texture);
    }
    memset(g, 0, sizeof(*g));
}
