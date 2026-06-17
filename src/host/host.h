// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
// host.h -- the host interface.
//
// A host owns the window/surface, the GLES context, the clock, and input, then
// drives the render core. This is the seam the wallpaper/layer-shell host swaps
// in later: it only provides its own run(). The dev host (host_glfw.c) is the one
// implementation for now.
//
// Writing another host (e.g. a wlr-layer-shell wallpaper). The render core
// (renderer.h) is host-agnostic and already complete; a new host must:
//   1. Create a surface and a *current* OpenGL ES 3.0 context (GLSL ES 3.00).
//      For a wallpaper that means a wl_surface on the background layer via
//      wlr-layer-shell, plus an EGL context made current on it.
//   2. Call tess_renderer_init(fb_w, fb_h, cfg) once the context is current.
//   3. Call tess_renderer_resize(w, h) whenever the drawable size changes.
//   4. Each frame: tess_renderer_draw(t) with t a monotonic wall clock in
//      seconds (the renderer applies speed internally), then swap buffers.
//      Honour cfg.fps_cap so an idle wallpaper does not cook the GPU.
//   5. tess_renderer_shutdown() at teardown.
// Optional niceties this dev host also does: poll the shader/config file mtimes
// for live edits (tess_renderer_reload_shader_if_changed / tess_config_load +
// tess_renderer_apply_config) and expose the glyph-blend toggle. None of the GL,
// shader, atlas, or uniform handling needs to be touched: it lives entirely in
// the core. The shader's uniform interface is documented in shaders/ascii.frag.

#ifndef TESS_HOST_H
#define TESS_HOST_H

#include "core/config.h"

/// Create the window + GLES 3.0 context, run the render loop until the user
/// quits, then tear everything down. Returns 0 on a clean exit, nonzero on a
/// setup failure (already logged).
///
/// `config_path` (may be NULL) is watched for live edits: when it changes on
/// disk it is re-parsed over the defaults and applied to the running effect. The
/// shader source files are watched the same way.
int tess_host_run(const tess_config *cfg, const char *config_path);

#endif // TESS_HOST_H
