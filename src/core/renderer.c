// renderer.c -- implementation of the host-agnostic render core (see renderer.h).

#include "core/renderer.h"
#include "core/shader.h"
#include "core/glyphs.h"

#include <GLES3/gl3.h>
#include <math.h>
#include <stdio.h>

// Shader source paths, relative to the working directory (the repo root for the
// dev host). Kept here for now; made configurable later (CP4).
#define VERT_PATH "shaders/fullscreen.vert"
#define FRAG_PATH "shaders/ascii.frag"

// Atlas oversampling: glyphs are baked at this multiple of the screen cell, then
// downsampled by the GPU. 2x is a clean LINEAR downsample without mipmaps.
#define GLYPH_SUPERSAMPLE 2

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
    GLint       u_resolution;
    GLint       u_time;
    GLint       u_cell;
    GLint       u_atlas;
    GLint       u_ramp_count;
    GLint       u_skip;
    bool        ready;
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
    g_rndr.u_resolution = tess_shader_uniform(&g_rndr.shader, "u_resolution");
    g_rndr.u_time       = tess_shader_uniform(&g_rndr.shader, "u_time");
    g_rndr.u_cell       = tess_shader_uniform(&g_rndr.shader, "u_cell");
    g_rndr.u_atlas      = tess_shader_uniform(&g_rndr.shader, "u_atlas");
    g_rndr.u_ramp_count = tess_shader_uniform(&g_rndr.shader, "u_ramp_count");
    g_rndr.u_skip       = tess_shader_uniform(&g_rndr.shader, "u_skip");
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
                           g_rndr.cfg.line_h_ratio, GLYPH_SUPERSAMPLE)) {
        return false;
    }
    tess_glyphs_destroy(&g_rndr.glyphs);
    g_rndr.glyphs = next;
    return true;
}

// --- Public API ------------------------------------------------------------

int tess_renderer_init(int w, int h, const tess_config *cfg) {
    g_rndr.cfg    = *cfg;       // copy; caller keeps ownership of its struct.
    g_rndr.width  = w;
    g_rndr.height = h;
    g_rndr.ready  = false;

    if (!tess_shader_load(&g_rndr.shader, VERT_PATH, FRAG_PATH)) {
        fprintf(stderr, "renderer: shader load failed\n");
        return 1;
    }
    cache_uniforms();

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
    int want_tile_w = (int)lroundf(g_rndr.cell_w * (float)GLYPH_SUPERSAMPLE);
    if (want_tile_w != g_rndr.glyphs.tile_w) {
        rebuild_glyphs();
    }
    glViewport(0, 0, w, h);
}

void tess_renderer_draw(double time_seconds) {
    if (!g_rndr.ready) {
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    tess_shader_use(&g_rndr.shader);

    glUniform2f(g_rndr.u_resolution, (float)g_rndr.width, (float)g_rndr.height);
    // Feed raw wall-clock seconds; the shader scales this into field-time and
    // owns the animation speed.
    glUniform1f(g_rndr.u_time, (float)time_seconds);
    glUniform2f(g_rndr.u_cell, g_rndr.cell_w, g_rndr.cell_h);
    glUniform1f(g_rndr.u_ramp_count, (float)g_rndr.glyphs.count);
    glUniform1f(g_rndr.u_skip, g_rndr.cfg.skip);

    // Glyph atlas on texture unit 0.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_rndr.glyphs.texture);
    glUniform1i(g_rndr.u_atlas, 0);

    // One fullscreen triangle, no buffers. The empty VAO is already bound.
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

int tess_renderer_reload_shader(void) {
    if (!tess_shader_reload(&g_rndr.shader)) {
        return 1;
    }
    cache_uniforms();
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
