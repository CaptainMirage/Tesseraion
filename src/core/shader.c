// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
// shader.c -- implementation of the GLSL program module (see shader.h).

// strdup is POSIX, not C11; request it explicitly before any include so the
// -std=c11 build still sees the declaration.
#define _POSIX_C_SOURCE 200809L

#include "core/shader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- File IO ---------------------------------------------------------------

/// Read an entire text file into a freshly malloc'd, NUL-terminated buffer.
/// Returns NULL (and logs) on any IO failure. Caller frees.
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "shader: cannot open '%s'\n", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "shader: seek failed on '%s'\n", path);
        fclose(f);
        return NULL;
    }
    long len = ftell(f);
    if (len < 0) {
        fprintf(stderr, "shader: tell failed on '%s'\n", path);
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {   // seek back to start (checked, unlike rewind).
        fprintf(stderr, "shader: rewind failed on '%s'\n", path);
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fprintf(stderr, "shader: out of memory reading '%s'\n", path);
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

// --- Compile / link --------------------------------------------------------

/// Compile one stage from source. Returns the shader object, or 0 on failure
/// after logging the compiler info log. Caller deletes the returned object.
static GLuint compile_stage(GLenum type, const char *src, const char *path) {
    GLuint s = glCreateShader(type);
    if (!s) {
        fprintf(stderr, "shader: glCreateShader failed for '%s'\n", path);
        return 0;
    }
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);

    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint log_len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &log_len);
        char *log = (log_len > 0) ? malloc((size_t)log_len) : NULL;
        if (log) {
            glGetShaderInfoLog(s, log_len, NULL, log);
            fprintf(stderr, "shader: compile failed for '%s':\n%s\n", path, log);
            free(log);
        } else {
            fprintf(stderr, "shader: compile failed for '%s' (no log)\n", path);
        }
        glDeleteShader(s);
        return 0;
    }
    return s;
}

/// Build a linked program from two source strings. Returns 0 on failure.
static GLuint build_program(const char *vert_src, const char *vert_path,
                            const char *frag_src, const char *frag_path) {
    GLuint vs = compile_stage(GL_VERTEX_SHADER, vert_src, vert_path);
    if (!vs) {
        return 0;
    }
    GLuint fs = compile_stage(GL_FRAGMENT_SHADER, frag_src, frag_path);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    if (!prog) {
        fprintf(stderr, "shader: glCreateProgram failed\n");
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    // Shaders can be detached/deleted once linked; the program keeps what it needs.
    glDetachShader(prog, vs);
    glDetachShader(prog, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint log_len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_len);
        char *log = (log_len > 0) ? malloc((size_t)log_len) : NULL;
        if (log) {
            glGetProgramInfoLog(prog, log_len, NULL, log);
            fprintf(stderr, "shader: link failed:\n%s\n", log);
            free(log);
        } else {
            fprintf(stderr, "shader: link failed (no log)\n");
        }
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

/// Read both source files and build a program. Returns 0 on any failure.
static GLuint build_from_paths(const char *vert_path, const char *frag_path) {
    char *vert_src = read_file(vert_path);
    if (!vert_src) {
        return 0;
    }
    char *frag_src = read_file(frag_path);
    if (!frag_src) {
        free(vert_src);
        return 0;
    }
    GLuint prog = build_program(vert_src, vert_path, frag_src, frag_path);
    free(vert_src);
    free(frag_src);
    return prog;
}

// --- Public API ------------------------------------------------------------

bool tess_shader_load(tess_shader *sh, const char *vert_path, const char *frag_path) {
    memset(sh, 0, sizeof(*sh));

    GLuint prog = build_from_paths(vert_path, frag_path);
    if (!prog) {
        return false;
    }

    sh->vert_path = strdup(vert_path);
    sh->frag_path = strdup(frag_path);
    if (!sh->vert_path || !sh->frag_path) {
        fprintf(stderr, "shader: out of memory storing paths\n");
        glDeleteProgram(prog);
        free(sh->vert_path);
        free(sh->frag_path);
        memset(sh, 0, sizeof(*sh));
        return false;
    }

    sh->program = prog;
    return true;
}

bool tess_shader_reload(tess_shader *sh) {
    if (!sh->vert_path || !sh->frag_path) {
        fprintf(stderr, "shader: reload on an unloaded shader\n");
        return false;
    }
    // Build into a fresh program first; only swap if the build succeeds so a
    // broken edit leaves the running program intact.
    GLuint prog = build_from_paths(sh->vert_path, sh->frag_path);
    if (!prog) {
        return false;
    }
    if (sh->program) {
        glDeleteProgram(sh->program);
    }
    sh->program = prog;
    return true;
}

void tess_shader_destroy(tess_shader *sh) {
    if (sh->program) {
        glDeleteProgram(sh->program);
    }
    free(sh->vert_path);
    free(sh->frag_path);
    memset(sh, 0, sizeof(*sh));
}

void tess_shader_use(const tess_shader *sh) {
    glUseProgram(sh->program);
}

GLint tess_shader_uniform(const tess_shader *sh, const char *name) {
    return glGetUniformLocation(sh->program, name);
}
