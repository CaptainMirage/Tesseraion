// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
// renderer.c -- implementation of the host-agnostic render core (see renderer.h).

#include "core/renderer.h"
#include "core/shader.h"
#include "core/glyphs.h"

#include <GLES3/gl3.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

// Shader source paths, relative to the working directory (the repo root for the
// dev host). Kept here for now; made configurable later (CP4).
#define VERT_PATH "shaders/fullscreen.vert"
#define FRAG_PATH "shaders/ascii.frag"

// --- Renderer state --------------------------------------------------------
// A single static instance: there is one render core per process (one context,
// one fullscreen pass). Keeping it file-local keeps the public API a flat set
// of free functions, which is what the hosts expect.
static struct {
    tess_config cfg;
    tess_shader shader;
    tess_glyphs glyphs;
    GLuint      vao;         ///< empty VAO; GLES3 core still requires one bound to draw.
    int         width;
    int         height;
    float       cell_w;      ///< screen cell size in px (derives from cfg + width).
    float       cell_h;
    // Per-frame / per-event uniforms.
    GLint       u_resolution;
    GLint       u_time;
    GLint       u_cell;
    GLint       u_atlas;
    GLint       u_ramp_count;
    GLint       u_glyph_blend;
    // Config-driven uniforms (set on init / config reload).
    GLint       u_noise_scale;
    GLint       u_warp;
    GLint       u_softness;
    GLint       u_seed;
    GLint       u_skip;
    GLint       u_alpha_cap;
    GLint       u_fade_band;
    GLint       u_mid_rgb;
    GLint       u_peak_rgb;
    GLint       u_blue_start;
    GLint       u_blue_full;
    GLint       u_accent_boost;
    bool        glyph_blend; ///< cross-fade adjacent ramp glyphs when true.
    bool        ready;
    // Field-time accumulator: integrate wall-clock deltas scaled by cfg.speed so
    // a speed change alters the rate, not the phase (no jump in the pattern).
    double      field_time;
    double      last_wall;
    bool        have_last_wall;
    // Source mtimes from the last shader (re)load, for change polling.
    time_t      vert_mtime;
    time_t      frag_mtime;
} g_rndr;

// --- GL error reporting ----------------------------------------------------

/// Drain and log the GL error queue. Returns true if any error was seen.
static bool gl_check(const char *where) {
    bool had = false;
    GLenum e;
    while ((e = glGetError()) != GL_NO_ERROR) {
        fprintf(stderr, "renderer: GL error 0x%04x at %s\n", e, where);
        had = true;
    }
    return had;
}

/// Re-query the uniform locations after a (re)build. Missing uniforms are -1,
/// which glUniform* silently ignores, so this is safe even mid-bringup.
static void cache_uniforms(void) {
    g_rndr.u_resolution  = tess_shader_uniform(&g_rndr.shader, "u_resolution");
    g_rndr.u_time        = tess_shader_uniform(&g_rndr.shader, "u_time");
    g_rndr.u_cell        = tess_shader_uniform(&g_rndr.shader, "u_cell");
    g_rndr.u_atlas       = tess_shader_uniform(&g_rndr.shader, "u_atlas");
    g_rndr.u_ramp_count  = tess_shader_uniform(&g_rndr.shader, "u_ramp_count");
    g_rndr.u_glyph_blend = tess_shader_uniform(&g_rndr.shader, "u_glyph_blend");
    g_rndr.u_noise_scale = tess_shader_uniform(&g_rndr.shader, "u_noise_scale");
    g_rndr.u_warp        = tess_shader_uniform(&g_rndr.shader, "u_warp");
    g_rndr.u_softness    = tess_shader_uniform(&g_rndr.shader, "u_softness");
    g_rndr.u_seed        = tess_shader_uniform(&g_rndr.shader, "u_seed");
    g_rndr.u_skip        = tess_shader_uniform(&g_rndr.shader, "u_skip");
    g_rndr.u_alpha_cap   = tess_shader_uniform(&g_rndr.shader, "u_alpha_cap");
    g_rndr.u_fade_band   = tess_shader_uniform(&g_rndr.shader, "u_fade_band");
    g_rndr.u_mid_rgb     = tess_shader_uniform(&g_rndr.shader, "u_mid_rgb");
    g_rndr.u_peak_rgb    = tess_shader_uniform(&g_rndr.shader, "u_peak_rgb");
    g_rndr.u_blue_start  = tess_shader_uniform(&g_rndr.shader, "u_blue_start");
    g_rndr.u_blue_full   = tess_shader_uniform(&g_rndr.shader, "u_blue_full");
    g_rndr.u_accent_boost = tess_shader_uniform(&g_rndr.shader, "u_accent_boost");
}

/// Push every config-driven uniform to the live program. Binds the program
/// first, so it is safe to call outside the draw path (init, reload). Colours go
/// to the shader normalized 0..1; the config keeps them human-friendly 0..255.
static void apply_config_uniforms(void) {
    const tess_config *c = &g_rndr.cfg;
    tess_shader_use(&g_rndr.shader);
    glUniform1f(g_rndr.u_noise_scale, c->noise_scale);
    glUniform1f(g_rndr.u_warp, c->warp);
    glUniform1f(g_rndr.u_softness, c->softness);
    glUniform2f(g_rndr.u_seed, c->seed_x, c->seed_y);
    glUniform1f(g_rndr.u_skip, c->skip);
    glUniform1f(g_rndr.u_alpha_cap, c->alpha_cap);
    glUniform1f(g_rndr.u_fade_band, c->fade_band);
    glUniform3f(g_rndr.u_mid_rgb,
                c->mid_rgb[0] / 255.0f, c->mid_rgb[1] / 255.0f, c->mid_rgb[2] / 255.0f);
    glUniform3f(g_rndr.u_peak_rgb,
                c->peak_rgb[0] / 255.0f, c->peak_rgb[1] / 255.0f, c->peak_rgb[2] / 255.0f);
    glUniform1f(g_rndr.u_blue_start, c->blue_start);
    glUniform1f(g_rndr.u_blue_full, c->blue_full);
    glUniform1f(g_rndr.u_accent_boost, c->accent_boost);
    glUniform1i(g_rndr.u_atlas, 0);   // sampler -> texture unit 0 (static binding).
}

/// Current mtime of a file, or 0 if it cannot be stat'd (treated as "absent").
static time_t file_mtime(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0) ? st.st_mtime : 0;
}

/// Record the shader source mtimes as of the latest load.
static void snapshot_shader_mtimes(void) {
    g_rndr.vert_mtime = file_mtime(VERT_PATH);
    g_rndr.frag_mtime = file_mtime(FRAG_PATH);
}

/// Derive the screen cell size from the config, growing it when the column count
/// would exceed max_cols (bounds the per-frame cell work on very wide screens).
/// Mirrors the web layout so the grid matches.
static void compute_cell(int w) {
    float font_px = g_rndr.cfg.font_size;
    float cw      = font_px * g_rndr.cfg.char_w_ratio;
    if (g_rndr.cfg.max_cols > 0 && (float)w / cw > (float)g_rndr.cfg.max_cols) {
        cw      = (float)w / (float)g_rndr.cfg.max_cols;
        font_px = cw / g_rndr.cfg.char_w_ratio;
    }
    g_rndr.cell_w = cw;
    g_rndr.cell_h = font_px * g_rndr.cfg.line_h_ratio;
}

/// (Re)bake the glyph atlas for the current cell size. Builds into a temp first
/// and only swaps on success, so a failed rebuild leaves the live atlas intact.
static bool rebuild_glyphs(void) {
    tess_glyphs next;
    if (!tess_glyphs_build(&next, g_rndr.cfg.ramp, g_rndr.cell_w, g_rndr.cell_h,
                           g_rndr.cfg.line_h_ratio, g_rndr.cfg.supersample,
                           (tess_glyph_filter)g_rndr.cfg.glyph_filter)) {
        return false;
    }
    tess_glyphs_destroy(&g_rndr.glyphs);
    g_rndr.glyphs = next;
    // The atlas stays bound on texture unit 0 for the renderer's lifetime; bind
    // the new one here (the only time it changes) so draw never has to.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_rndr.glyphs.texture);
    return true;
}

// --- Public API ------------------------------------------------------------

int tess_renderer_init(int w, int h, const tess_config *cfg) {
    g_rndr.cfg    = *cfg;       // copy; caller keeps ownership of its struct.
    g_rndr.width  = w;
    g_rndr.height = h;
    g_rndr.ready  = false;
    g_rndr.glyph_blend = true;  // smooth ramp transitions by default; toggleable.
    g_rndr.field_time     = 0.0;
    g_rndr.have_last_wall = false;

    if (!tess_shader_load(&g_rndr.shader, VERT_PATH, FRAG_PATH)) {
        fprintf(stderr, "renderer: shader load failed\n");
        return 1;
    }
    cache_uniforms();
    apply_config_uniforms();
    snapshot_shader_mtimes();

    // Derive the cell size, then bake the ramp atlas to match it (crisp on screen).
    compute_cell(w);
    if (!rebuild_glyphs()) {
        fprintf(stderr, "renderer: glyph atlas build failed\n");
        tess_shader_destroy(&g_rndr.shader);
        return 1;
    }

    // Empty VAO: the fullscreen triangle is generated from gl_VertexID, so it
    // has no attributes, but GLES3 core still requires a bound VAO to draw.
    glGenVertexArrays(1, &g_rndr.vao);
    glBindVertexArray(g_rndr.vao);

    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);   // pure black, matching the site bg.
    glDisable(GL_DEPTH_TEST);               // a 2D fullscreen pass needs no depth.

    if (gl_check("init")) {
        return 1;
    }
    g_rndr.ready = true;
    return 0;
}

void tess_renderer_resize(int w, int h) {
    g_rndr.width  = w;
    g_rndr.height = h;
    compute_cell(w);
    // Rebuild the atlas only if the baked tile size would actually change, so a
    // resize drag does not thrash the GPU. A failed rebuild keeps the old atlas.
    int ss = g_rndr.cfg.supersample;
    if (ss < 1) { ss = 1; } else if (ss > 8) { ss = 8; }   // match glyphs_build clamp.
    int want_tile_w = (int)lroundf(g_rndr.cell_w * (float)ss);
    if (want_tile_w != g_rndr.glyphs.tile_w) {
        rebuild_glyphs();
    }
    glViewport(0, 0, w, h);
}

void tess_renderer_draw(double time_seconds) {
    if (!g_rndr.ready) {
        return;
    }
    // Nothing to draw at zero size (e.g. a minimized window or a compositor
    // transition); skip the pass and avoid a degenerate 0-cell grid.
    if (g_rndr.width <= 0 || g_rndr.height <= 0) {
        return;
    }

    // Integrate field-time from the wall-clock delta scaled by the current speed.
    // Accumulating (rather than field_time = wall * speed) means a live speed
    // change alters the drift rate without jumping the pattern to a new phase.
    if (!g_rndr.have_last_wall) {
        g_rndr.last_wall = time_seconds;
        g_rndr.have_last_wall = true;
    }
    g_rndr.field_time += (time_seconds - g_rndr.last_wall) * g_rndr.cfg.speed;
    g_rndr.last_wall = time_seconds;

    glClear(GL_COLOR_BUFFER_BIT);
    tess_shader_use(&g_rndr.shader);

    // Per-frame / per-event uniforms only. The config-driven uniforms, the sampler
    // binding, and the atlas texture are all set once (init / reload / atlas
    // rebuild), so the hot path stays a handful of cheap uniform writes plus one
    // draw, with no heap allocation and no binding churn.
    glUniform2f(g_rndr.u_resolution, (float)g_rndr.width, (float)g_rndr.height);
    glUniform1f(g_rndr.u_time, (float)g_rndr.field_time);   // field-time (speed applied).
    glUniform2f(g_rndr.u_cell, g_rndr.cell_w, g_rndr.cell_h);
    glUniform1f(g_rndr.u_ramp_count, (float)g_rndr.glyphs.count);
    glUniform1f(g_rndr.u_glyph_blend, g_rndr.glyph_blend ? 1.0f : 0.0f);

    // One fullscreen triangle, no buffers. The empty VAO and the atlas (unit 0)
    // are already bound.
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void tess_renderer_set_glyph_blend(bool on) {
    g_rndr.glyph_blend = on;
}

bool tess_renderer_glyph_blend(void) {
    return g_rndr.glyph_blend;
}

int tess_renderer_reload_shader(void) {
    // Snapshot first: even on a failed reload we do not want to retrigger on the
    // same unchanged-but-broken file every poll.
    snapshot_shader_mtimes();
    if (!tess_shader_reload(&g_rndr.shader)) {
        return 1;
    }
    cache_uniforms();
    apply_config_uniforms();
    return 0;
}

bool tess_renderer_reload_shader_if_changed(void) {
    if (file_mtime(VERT_PATH) == g_rndr.vert_mtime &&
        file_mtime(FRAG_PATH) == g_rndr.frag_mtime) {
        return false;
    }
    if (tess_renderer_reload_shader() == 0) {
        fprintf(stderr, "renderer: shader reloaded\n");
    }
    return true;
}

int tess_renderer_apply_config(const tess_config *cfg) {
    g_rndr.cfg = *cfg;
    compute_cell(g_rndr.width);
    // Ramp or cell metrics may have changed, so rebuild the atlas; keep the old
    // one if the rebuild fails.
    if (!rebuild_glyphs()) {
        fprintf(stderr, "renderer: atlas rebuild on config reload failed\n");
        apply_config_uniforms();
        return 1;
    }
    apply_config_uniforms();
    return 0;
}

void tess_renderer_shutdown(void) {
    if (g_rndr.vao) {
        glDeleteVertexArrays(1, &g_rndr.vao);
        g_rndr.vao = 0;
    }
    tess_glyphs_destroy(&g_rndr.glyphs);
    tess_shader_destroy(&g_rndr.shader);
    g_rndr.ready = false;
}
