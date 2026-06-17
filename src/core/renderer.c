// renderer.c -- implementation of the host-agnostic render core (see renderer.h).

#include "core/renderer.h"
#include "core/shader.h"

#include <GLES3/gl3.h>
#include <stdio.h>

// Shader source paths, relative to the working directory (the repo root for the
// dev host). Kept here for now; a later chunk can make these configurable.
#define VERT_PATH "shaders/fullscreen.vert"
#define FRAG_PATH "shaders/ascii.frag"

// --- Renderer state --------------------------------------------------------
// A single static instance: there is one render core per process (one context,
// one fullscreen pass). Keeping it file-local keeps the public API a flat set
// of free functions, which is what the hosts expect.
static struct {
    tess_config cfg;
    tess_shader shader;
    GLuint      vao;         ///< empty VAO; GLES3 core still requires one bound to draw.
    int         width;
    int         height;
    GLint       u_resolution;
    GLint       u_time;
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
    glViewport(0, 0, w, h);
}

void tess_renderer_draw(double time_seconds) {
    if (!g_rndr.ready) {
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    tess_shader_use(&g_rndr.shader);

    glUniform2f(g_rndr.u_resolution, (float)g_rndr.width, (float)g_rndr.height);
    // CP0 feeds raw seconds; the speed multiplier is applied in the field stage
    // at CP1 where it actually drives motion.
    glUniform1f(g_rndr.u_time, (float)time_seconds);

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
    tess_shader_destroy(&g_rndr.shader);
    g_rndr.ready = false;
}
