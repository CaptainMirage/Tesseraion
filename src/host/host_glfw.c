// host_glfw.c -- windowed dev host: GLFW window + GLES 3.0 context + loop.
//
// This is the only place that knows about GLFW. It creates the context, owns
// timing and input, and drives the render core through the four-call API
// (init/resize/draw/shutdown). Nothing here leaks into core/, so a layer-shell
// wallpaper host can replace this file wholesale by supplying its own GLES
// context and calling the same renderer functions.

#include "host/host.h"
#include "core/renderer.h"

// GLFW must see that we want the GLES headers, not desktop GL, before include.
#define GLFW_INCLUDE_ES3
#include <GLFW/glfw3.h>

#include <stdio.h>

#define INITIAL_WIDTH  1280
#define INITIAL_HEIGHT 720
#define WINDOW_TITLE   "Tesseraion (dev host)"

// --- GLFW callbacks --------------------------------------------------------

/// GLFW reports errors through this; surface them so context-creation failures
/// are not silent.
static void on_glfw_error(int code, const char *desc) {
    fprintf(stderr, "glfw: error %d: %s\n", code, desc);
}

/// Framebuffer (not window) size changes drive the GLES viewport, so hi-dpi
/// scaling is handled correctly.
static void on_framebuffer_size(GLFWwindow *win, int w, int h) {
    (void)win;
    tess_renderer_resize(w, h);
}

/// Esc quits; B toggles smooth glyph cross-fading. A live-reload key gets wired
/// alongside hot reload later (CP4).
static void on_key(GLFWwindow *win, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    if (action != GLFW_PRESS) {
        return;
    }
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
    } else if (key == GLFW_KEY_B) {
        bool on = !tess_renderer_glyph_blend();
        tess_renderer_set_glyph_blend(on);
        fprintf(stderr, "glyph blend: %s\n", on ? "on" : "off");
    }
}

// --- Host loop -------------------------------------------------------------

int tess_host_run(const tess_config *cfg) {
    glfwSetErrorCallback(on_glfw_error);

    if (!glfwInit()) {
        fprintf(stderr, "host: glfwInit failed\n");
        return 1;
    }

    // Request an OpenGL ES 3.0 context (matches WebGL2 / GLSL ES 3.00).
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow *win = glfwCreateWindow(INITIAL_WIDTH, INITIAL_HEIGHT,
                                       WINDOW_TITLE, NULL, NULL);
    if (!win) {
        fprintf(stderr, "host: window/context creation failed (no GLES 3.0?)\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(win);
    // Drive the frame rate from our own FPS cap, not vsync, so an idle wallpaper
    // can throttle below the display refresh.
    glfwSwapInterval(0);

    glfwSetFramebufferSizeCallback(win, on_framebuffer_size);
    glfwSetKeyCallback(win, on_key);

    // Use the real framebuffer size (hi-dpi aware) for the initial viewport.
    int fb_w = 0, fb_h = 0;
    glfwGetFramebufferSize(win, &fb_w, &fb_h);

    if (tess_renderer_init(fb_w, fb_h, cfg) != 0) {
        fprintf(stderr, "host: renderer init failed\n");
        glfwDestroyWindow(win);
        glfwTerminate();
        return 1;
    }

    fprintf(stderr, "tesseraion: Esc to quit, B to toggle glyph blend\n");

    // Frame pacing: render when the per-frame budget has elapsed, otherwise
    // sleep on events. This keeps CPU/GPU idle between frames (the FPS cap) yet
    // stays responsive to resize/close.
    const double budget = (cfg->fps_cap > 0) ? 1.0 / (double)cfg->fps_cap : 0.0;
    double next = glfwGetTime();

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        double now = glfwGetTime();
        if (now >= next) {
            tess_renderer_draw(now);
            glfwSwapBuffers(win);
            next += budget;
            // If we fell behind (e.g. after a stall), resync instead of
            // spiraling through a backlog of catch-up frames.
            if (next < now) {
                next = now + budget;
            }
        } else {
            // Sleep the remaining budget; wakes early on any input event.
            glfwWaitEventsTimeout(next - now);
        }
    }

    tess_renderer_shutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
