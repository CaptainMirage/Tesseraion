// renderer.h -- the host-agnostic GLES render core.
//
// This is the whole public surface a host talks to. The renderer assumes a
// current OpenGL ES 3.0 context already exists (the host created it); it never
// touches windowing, input, or timing. A host calls init once, resize on every
// framebuffer size change, draw once per frame with the current clock, and
// shutdown at the end. The dev host (GLFW) and the future layer-shell wallpaper
// host both drive the renderer through exactly these four calls.
//
// The renderer draws one fullscreen triangle (no VBO, an empty VAO) and feeds
// the shader the documented uniform contract (see shaders/ascii.frag).

#ifndef TESS_RENDERER_H
#define TESS_RENDERER_H

#include "core/config.h"

/// Initialize the render core for a w x h framebuffer. cfg is copied, so the
/// caller may free or reuse it afterwards. A current GLES 3.0 context must be
/// active. Returns 0 on success, nonzero on failure (already logged).
int tess_renderer_init(int w, int h, const tess_config *cfg);

/// React to a framebuffer resize (updates the viewport and cached resolution).
void tess_renderer_resize(int w, int h);

/// Render one frame. time_seconds is the host's clock; the renderer applies the
/// configured speed when it feeds the shader.
void tess_renderer_draw(double time_seconds);

/// Rebuild the shader program from disk (hot reload). Returns 0 on success,
/// nonzero if the rebuild failed (the previous program stays live).
int tess_renderer_reload_shader(void);

/// Release all GL resources. Safe to call even if init failed.
void tess_renderer_shutdown(void);

#endif // TESS_RENDERER_H
