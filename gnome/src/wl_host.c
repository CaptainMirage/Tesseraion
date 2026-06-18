// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
// wl_host.c -- GNOME wallpaper host: raw wayland-client + EGL + xdg-shell.
//
// This is the only place that knows about Wayland and EGL. It binds the registry
// globals (wl_compositor, xdg_wm_base), creates a toplevel surface with a stable
// app_id, brings up an EGL OpenGL ES 3.0 context on a wl_egl_window, and drives the
// render core through the four-call API (init/resize/draw/shutdown). Nothing here
// leaks into core/: the GNOME Shell extension reparents this window's actor into the
// desktop background, so to the core this is just another host that owns a surface,
// a context, the clock, and the frame pacing.
//
// CP1 scope: stand the window + context up and render the effect into a normal
// toplevel. Backgrounding it (the extension), multi-monitor sizing, and lifecycle
// pausing are later chunks.

// clock_gettime/CLOCK_MONOTONIC and poll are POSIX, not C11; request them explicitly
// before any include so the -std=c11 build still sees the declarations.
#define _POSIX_C_SOURCE 200809L

#include "wl_host.h"
#include "core/config.h"
#include "core/renderer.h"

#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "xdg-shell-client-protocol.h"

#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// Fallback window size used until (and if) the compositor suggests one. A normal
// toplevel configure for a wallpaper-style window usually leaves the size to us.
#define INITIAL_WIDTH  1280
#define INITIAL_HEIGHT 720
#define WINDOW_TITLE   "Tesseraion (GNOME wallpaper host)"

// --- Host state ------------------------------------------------------------
// One host per process. Kept file-local so the registry/listener callbacks can
// reach it through the wl_data user pointer without scattering globals.
typedef struct {
    // Wayland core objects.
    struct wl_display    *display;
    struct wl_registry   *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base   *wm_base;
    struct wl_surface    *surface;
    struct xdg_surface   *xdg_surface;
    struct xdg_toplevel  *toplevel;

    // EGL + the drawable that wraps the wl_surface.
    EGLDisplay            egl_display;
    EGLConfig             egl_config;
    EGLContext            egl_context;
    EGLSurface            egl_surface;
    struct wl_egl_window *egl_window;

    int  width;
    int  height;
    bool configured;     ///< first xdg_surface configure has been ack'd.
    bool should_close;   ///< toplevel close requested, or a fatal error.

    // Pending size from the most recent toplevel configure, applied atomically on
    // the following xdg_surface configure (the standard xdg-shell sequencing).
    int  pending_width;
    int  pending_height;
} wl_host;

// --- Time helper -----------------------------------------------------------

/// Monotonic wall clock in seconds, the steadily increasing time the renderer wants
/// (it applies the speed multiplier internally).
static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/// Current mtime of a file, or 0 if it cannot be stat'd (treated as "absent").
static time_t file_mtime(const char *path) {
    struct stat st;
    return (path && stat(path, &st) == 0) ? st.st_mtime : 0;
}

// --- xdg_wm_base: respond to pings so the compositor keeps us alive ----------

static void on_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = on_wm_base_ping,
};

// --- xdg_toplevel: size + close -------------------------------------------

/// The compositor proposes a size (0 means "you choose"). We stash it and apply it
/// when the matching xdg_surface configure arrives, per the xdg-shell contract.
static void on_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                                  int32_t width, int32_t height,
                                  struct wl_array *states) {
    (void)toplevel;
    (void)states;
    wl_host *h = data;
    if (width > 0 && height > 0) {
        h->pending_width  = width;
        h->pending_height = height;
    }
}

static void on_toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    (void)toplevel;
    ((wl_host *)data)->should_close = true;
}

// GNOME's mutter sends configure_bounds (and, on newer protocol, wm_capabilities).
// We do not need them; empty handlers keep the listener struct complete and
// forward-compatible without reacting.
static void on_toplevel_configure_bounds(void *data, struct xdg_toplevel *toplevel,
                                         int32_t width, int32_t height) {
    (void)data; (void)toplevel; (void)width; (void)height;
}

static void on_toplevel_wm_capabilities(void *data, struct xdg_toplevel *toplevel,
                                        struct wl_array *capabilities) {
    (void)data; (void)toplevel; (void)capabilities;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure        = on_toplevel_configure,
    .close            = on_toplevel_close,
    .configure_bounds = on_toplevel_configure_bounds,
    .wm_capabilities  = on_toplevel_wm_capabilities,
};

// --- xdg_surface: ack configure, apply any pending size ---------------------

static void on_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                     uint32_t serial) {
    wl_host *h = data;
    xdg_surface_ack_configure(xdg_surface, serial);

    // Apply a newly proposed size (only after EGL is up; before that we just adopt
    // it as the starting size). The renderer's resize updates the viewport and cell.
    if (h->pending_width > 0 && h->pending_height > 0 &&
        (h->pending_width != h->width || h->pending_height != h->height)) {
        h->width  = h->pending_width;
        h->height = h->pending_height;
        if (h->egl_window) {
            wl_egl_window_resize(h->egl_window, h->width, h->height, 0, 0);
            tess_renderer_resize(h->width, h->height);
        }
    }
    h->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = on_xdg_surface_configure,
};

// --- Registry: bind the globals we need ------------------------------------

static void on_registry_global(void *data, struct wl_registry *registry,
                               uint32_t name, const char *interface,
                               uint32_t version) {
    wl_host *h = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        // Bind a modern-enough compositor for wl_surface; cap at 4 (ample for us).
        uint32_t want = version < 4 ? version : 4;
        h->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, want);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        uint32_t want = version < 2 ? version : 2;
        h->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, want);
        xdg_wm_base_add_listener(h->wm_base, &wm_base_listener, h);
    }
}

static void on_registry_global_remove(void *data, struct wl_registry *registry,
                                      uint32_t name) {
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global        = on_registry_global,
    .global_remove = on_registry_global_remove,
};

// --- EGL bring-up ----------------------------------------------------------

/// Create the EGL display/config/context for OpenGL ES 3.0 on the Wayland display.
/// Returns true on success; logs and returns false otherwise.
static bool egl_init(wl_host *h) {
    // EGL 1.5 platform query: get the display for our Wayland connection.
    h->egl_display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, h->display, NULL);
    if (h->egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "host: eglGetPlatformDisplay failed\n");
        return false;
    }

    EGLint major = 0, minor = 0;
    if (!eglInitialize(h->egl_display, &major, &minor)) {
        fprintf(stderr, "host: eglInitialize failed\n");
        return false;
    }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        fprintf(stderr, "host: eglBindAPI(ES) failed\n");
        return false;
    }

    // RGBA8 window config renderable by OpenGL ES 3 (ES3 bit covers our GLSL ES 3.00).
    const EGLint config_attrs[] = {
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_NONE,
    };
    EGLint num_configs = 0;
    if (!eglChooseConfig(h->egl_display, config_attrs, &h->egl_config, 1, &num_configs) ||
        num_configs < 1) {
        fprintf(stderr, "host: no suitable EGL config (need GLES3 RGBA8 window)\n");
        return false;
    }

    const EGLint context_attrs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_NONE,
    };
    h->egl_context = eglCreateContext(h->egl_display, h->egl_config,
                                      EGL_NO_CONTEXT, context_attrs);
    if (h->egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "host: eglCreateContext (GLES 3.0) failed\n");
        return false;
    }
    return true;
}

/// Wrap the wl_surface in a wl_egl_window and an EGL window surface, then make the
/// context current. Must run after the first xdg_surface configure.
static bool egl_make_surface(wl_host *h) {
    h->egl_window = wl_egl_window_create(h->surface, h->width, h->height);
    if (!h->egl_window) {
        fprintf(stderr, "host: wl_egl_window_create failed\n");
        return false;
    }
    h->egl_surface = eglCreateWindowSurface(h->egl_display, h->egl_config,
                                            (EGLNativeWindowType)h->egl_window, NULL);
    if (h->egl_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "host: eglCreateWindowSurface failed\n");
        return false;
    }
    if (!eglMakeCurrent(h->egl_display, h->egl_surface, h->egl_surface,
                        h->egl_context)) {
        fprintf(stderr, "host: eglMakeCurrent failed\n");
        return false;
    }
    // Drive the frame rate from our own FPS cap, not vsync, so an idle wallpaper can
    // throttle below the display refresh (matches the dev host's swapInterval(0)).
    eglSwapInterval(h->egl_display, 0);
    return true;
}

// --- Teardown --------------------------------------------------------------

static void host_destroy(wl_host *h) {
    if (h->egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(h->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (h->egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(h->egl_display, h->egl_surface);
        }
        if (h->egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(h->egl_display, h->egl_context);
        }
    }
    if (h->egl_window)  { wl_egl_window_destroy(h->egl_window); }
    if (h->egl_display != EGL_NO_DISPLAY) { eglTerminate(h->egl_display); }

    if (h->toplevel)    { xdg_toplevel_destroy(h->toplevel); }
    if (h->xdg_surface) { xdg_surface_destroy(h->xdg_surface); }
    if (h->surface)     { wl_surface_destroy(h->surface); }
    if (h->wm_base)     { xdg_wm_base_destroy(h->wm_base); }
    if (h->compositor)  { wl_compositor_destroy(h->compositor); }
    if (h->registry)    { wl_registry_destroy(h->registry); }
    if (h->display)     { wl_display_disconnect(h->display); }
}

// --- Public entry ----------------------------------------------------------

int tess_gnome_host_run(const tess_config *cfg, const char *config_path) {
    // Mutable copy so a live config edit can be re-applied and the frame budget
    // recomputed without disturbing the caller's struct (mirrors the dev host).
    tess_config live = *cfg;

    wl_host h;
    memset(&h, 0, sizeof h);
    h.egl_display = EGL_NO_DISPLAY;
    h.egl_context = EGL_NO_CONTEXT;
    h.egl_surface = EGL_NO_SURFACE;
    h.width  = INITIAL_WIDTH;
    h.height = INITIAL_HEIGHT;

    h.display = wl_display_connect(NULL);
    if (!h.display) {
        fprintf(stderr, "host: wl_display_connect failed (no Wayland session?)\n");
        return 1;
    }

    // Bind the registry globals, then round-trip so compositor + wm_base are ready.
    h.registry = wl_display_get_registry(h.display);
    wl_registry_add_listener(h.registry, &registry_listener, &h);
    wl_display_roundtrip(h.display);
    if (!h.compositor || !h.wm_base) {
        fprintf(stderr, "host: missing wl_compositor or xdg_wm_base\n");
        host_destroy(&h);
        return 1;
    }

    // Build the toplevel: surface -> xdg_surface -> xdg_toplevel, with the app_id the
    // extension matches on. An initial commit (no buffer) triggers the first
    // configure, which we round-trip for and ack before bringing EGL up.
    h.surface = wl_compositor_create_surface(h.compositor);
    h.xdg_surface = xdg_wm_base_get_xdg_surface(h.wm_base, h.surface);
    xdg_surface_add_listener(h.xdg_surface, &xdg_surface_listener, &h);
    h.toplevel = xdg_surface_get_toplevel(h.xdg_surface);
    xdg_toplevel_add_listener(h.toplevel, &toplevel_listener, &h);
    xdg_toplevel_set_app_id(h.toplevel, TESS_GNOME_APP_ID);
    xdg_toplevel_set_title(h.toplevel, WINDOW_TITLE);
    wl_surface_commit(h.surface);

    while (!h.configured && !h.should_close) {
        if (wl_display_dispatch(h.display) < 0) {
            fprintf(stderr, "host: wl_display_dispatch failed during bring-up\n");
            host_destroy(&h);
            return 1;
        }
    }

    if (!egl_init(&h) || !egl_make_surface(&h)) {
        host_destroy(&h);
        return 1;
    }

    if (tess_renderer_init(h.width, h.height, &live) != 0) {
        fprintf(stderr, "host: renderer init failed\n");
        host_destroy(&h);
        return 1;
    }

    fprintf(stderr, "tesseraion-gnome: app_id '%s'; shader/config hot-reload on save\n",
            TESS_GNOME_APP_ID);

    // Frame pacing: render when the per-frame budget has elapsed, otherwise sleep on
    // the Wayland fd until the next deadline (or an incoming event). Keeps the GPU
    // idle between frames at the configured FPS cap while staying responsive.
    double budget = (live.fps_cap > 0) ? 1.0 / (double)live.fps_cap : 0.0;
    double next   = now_seconds();

    // Live-edit watch: poll source mtimes a few times a second (not every frame).
    time_t cfg_mtime         = file_mtime(config_path);
    double next_reload_check = now_seconds();

    int fd = wl_display_get_fd(h.display);

    while (!h.should_close) {
        // Drain any already-queued events without blocking, then flush our requests.
        if (wl_display_dispatch_pending(h.display) < 0) {
            fprintf(stderr, "host: wl_display_dispatch_pending failed\n");
            break;
        }
        wl_display_flush(h.display);

        double now = now_seconds();

        if (now >= next_reload_check) {
            next_reload_check = now + 0.25;
            tess_renderer_reload_shader_if_changed();
            time_t m = file_mtime(config_path);
            if (config_path && m != cfg_mtime) {
                cfg_mtime = m;
                // Re-parse over fresh defaults; preserve the running seed unless the
                // file pins one, so a tweak does not jump the pattern.
                float prev_x = live.seed_x, prev_y = live.seed_y;
                tess_config_default(&live);
                tess_config_load(&live, config_path);
                if (!live.seed_pinned) {
                    live.seed_x = prev_x;
                    live.seed_y = prev_y;
                }
                tess_renderer_apply_config(&live);
                budget = (live.fps_cap > 0) ? 1.0 / (double)live.fps_cap : 0.0;
                fprintf(stderr, "config: reloaded %s\n", config_path);
            }
        }

        if (now >= next) {
            tess_renderer_draw(now);
            if (!eglSwapBuffers(h.egl_display, h.egl_surface)) {
                fprintf(stderr, "host: eglSwapBuffers failed\n");
                break;
            }
            next += budget;
            // If we fell behind (e.g. after a stall), resync instead of spiraling
            // through a backlog of catch-up frames.
            if (next < now) {
                next = now + budget;
            }
        } else {
            // Sleep until the next frame deadline, waking early on a Wayland event.
            int timeout_ms = (int)((next - now) * 1000.0);
            if (timeout_ms < 0) { timeout_ms = 0; }
            struct pollfd pfd = { .fd = fd, .events = POLLIN };
            if (poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLIN)) {
                if (wl_display_dispatch(h.display) < 0) {
                    fprintf(stderr, "host: wl_display_dispatch failed\n");
                    break;
                }
            }
        }
    }

    tess_renderer_shutdown();
    host_destroy(&h);
    return 0;
}
