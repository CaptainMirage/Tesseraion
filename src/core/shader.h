// shader.h -- GLSL program loading, compiling, linking, and hot-reload.
//
// Host-agnostic: assumes a current GLES 3.0 context already exists. A
// tess_shader owns one program built from a vertex + fragment source file pair
// and remembers those paths so it can be rebuilt in place (used for hot reload, CP4).
// Uniform-location lookups go through helpers here so callers never cache stale
// locations across a reload.

#ifndef TESS_SHADER_H
#define TESS_SHADER_H

#include <GLES3/gl3.h>
#include <stdbool.h>

/// A compiled+linked GLSL program plus the source paths it was built from.
typedef struct {
    GLuint program;          ///< 0 until a successful build.
    char  *vert_path;        ///< owned copy of the vertex shader path.
    char  *frag_path;        ///< owned copy of the fragment shader path.
} tess_shader;

/// Build a program from two source files. On success the struct owns a live GL
/// program and copies of both paths. Returns true on success; on failure the
/// struct is left zeroed, the error is logged, and false is returned.
bool tess_shader_load(tess_shader *sh, const char *vert_path, const char *frag_path);

/// Rebuild the program from its stored paths. On success the old program is
/// replaced and deleted. On failure the existing program is kept untouched (so a
/// bad edit during hot reload never blanks the screen) and false is returned.
bool tess_shader_reload(tess_shader *sh);

/// Delete the program and free owned paths. Safe to call on a zeroed struct.
void tess_shader_destroy(tess_shader *sh);

/// glUseProgram wrapper for readability at call sites.
void tess_shader_use(const tess_shader *sh);

/// Look up a uniform location by name (-1 if absent). Cheap enough to call per
/// frame for the handful of uniforms this project uses; cache later if needed.
GLint tess_shader_uniform(const tess_shader *sh, const char *name);

#endif // TESS_SHADER_H
