// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
// config.c -- defaults and file parsing for the render-core tunables (config.h).

#include "core/config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tess_config_default(tess_config *cfg) {
    // Defaults from the original website background effect.
    cfg->fps_cap   = 60;
    cfg->speed     = 1.0;   // field-units/sec; the calm, visibly-alive rate.

    cfg->font_size    = 10.0f;
    cfg->char_w_ratio = 0.62f;
    cfg->line_h_ratio = 1.4f;
    cfg->max_cols     = 260;

    cfg->supersample  = 4;   // matches TESS_GLYPH_SMOOTH trilinear default.
    cfg->glyph_filter = 0;   // TESS_GLYPH_SMOOTH.

    snprintf(cfg->ramp, sizeof cfg->ramp, "%s", " .:-=+*#%@");

    cfg->noise_scale = 3.3f;
    cfg->warp        = 0.6f;
    cfg->softness    = 1.2f;
    cfg->skip        = 0.28f;
    cfg->alpha_cap   = 0.50f;
    cfg->fade_band   = 0.22f;

    cfg->seed_x      = 0.0f;
    cfg->seed_y      = 0.0f;
    cfg->seed_pinned = false;

    cfg->mid_rgb[0]  = 120.0f; cfg->mid_rgb[1]  = 128.0f; cfg->mid_rgb[2]  = 148.0f;
    cfg->peak_rgb[0] =  74.0f; cfg->peak_rgb[1] = 140.0f; cfg->peak_rgb[2] = 255.0f;
    cfg->blue_start   = 0.60f;
    cfg->blue_full    = 0.93f;
    cfg->accent_boost = 0.30f;
}

// --- File parsing ----------------------------------------------------------

/// Trim leading/trailing ASCII whitespace in place, returning the new start.
static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return s;
}

/// Parse `val` as a float into *out; returns false (and leaves *out) on garbage.
static bool parse_float(const char *val, float *out) {
    char *end = NULL;
    float f = strtof(val, &end);
    if (end == val) {
        return false;
    }
    *out = f;
    return true;
}

/// Parse three whitespace-separated floats (an rgb triple) into out[3].
static bool parse_rgb(const char *val, float out[3]) {
    char *end = NULL;
    const char *p = val;
    for (int i = 0; i < 3; i++) {
        float f = strtof(p, &end);
        if (end == p) {
            return false;
        }
        out[i] = f;
        p = end;
    }
    return true;
}

/// Copy a ramp value into cfg->ramp, stripping one optional pair of surrounding
/// double quotes so leading/trailing spaces in the ramp are preserved.
static void parse_ramp(const char *val, char *dst, size_t cap) {
    size_t len = strlen(val);
    if (len >= 2 && val[0] == '"' && val[len - 1] == '"') {
        val++;
        len -= 2;
    }
    if (len >= cap) {
        len = cap - 1;
    }
    memcpy(dst, val, len);
    dst[len] = '\0';
}

/// Apply one key=value pair to cfg. Logs and ignores unknown keys / bad values.
static void apply_kv(tess_config *cfg, const char *key, const char *val, int line) {
    float f;
    if      (strcmp(key, "fps_cap") == 0)      { if (parse_float(val, &f)) cfg->fps_cap = (int)f; }
    else if (strcmp(key, "speed") == 0)        { if (parse_float(val, &f)) cfg->speed = f; }
    else if (strcmp(key, "font_size") == 0)    { parse_float(val, &cfg->font_size); }
    else if (strcmp(key, "char_w_ratio") == 0) { parse_float(val, &cfg->char_w_ratio); }
    else if (strcmp(key, "line_h_ratio") == 0) { parse_float(val, &cfg->line_h_ratio); }
    else if (strcmp(key, "max_cols") == 0)     { if (parse_float(val, &f)) cfg->max_cols = (int)f; }
    else if (strcmp(key, "supersample") == 0)  { if (parse_float(val, &f)) cfg->supersample = (int)f; }
    else if (strcmp(key, "glyph_filter") == 0) {
        if      (strcmp(val, "smooth") == 0) { cfg->glyph_filter = 0; }
        else if (strcmp(val, "sharp")  == 0) { cfg->glyph_filter = 1; }
        else if (strcmp(val, "pixel")  == 0) { cfg->glyph_filter = 2; }
        else { fprintf(stderr, "config: glyph_filter '%s' not smooth/sharp/pixel on line %d "
                               "(ignored)\n", val, line); }
    }
    else if (strcmp(key, "ramp") == 0)         { parse_ramp(val, cfg->ramp, sizeof cfg->ramp); }
    else if (strcmp(key, "noise_scale") == 0)  { parse_float(val, &cfg->noise_scale); }
    else if (strcmp(key, "warp") == 0)         { parse_float(val, &cfg->warp); }
    else if (strcmp(key, "softness") == 0)     { parse_float(val, &cfg->softness); }
    else if (strcmp(key, "skip") == 0)         { parse_float(val, &cfg->skip); }
    else if (strcmp(key, "alpha_cap") == 0)    { parse_float(val, &cfg->alpha_cap); }
    else if (strcmp(key, "fade_band") == 0)    { parse_float(val, &cfg->fade_band); }
    else if (strcmp(key, "mid_rgb") == 0)      { parse_rgb(val, cfg->mid_rgb); }
    else if (strcmp(key, "peak_rgb") == 0)     { parse_rgb(val, cfg->peak_rgb); }
    else if (strcmp(key, "blue_start") == 0)   { parse_float(val, &cfg->blue_start); }
    else if (strcmp(key, "blue_full") == 0)    { parse_float(val, &cfg->blue_full); }
    else if (strcmp(key, "accent_boost") == 0) { parse_float(val, &cfg->accent_boost); }
    else if (strcmp(key, "seed") == 0) {
        // A single scalar pins the pattern; derive the y offset like the original.
        if (parse_float(val, &f)) {
            cfg->seed_x = f;
            cfg->seed_y = f * 1.7f + 13.0f;
            cfg->seed_pinned = true;
        }
    } else {
        fprintf(stderr, "config: unknown key '%s' on line %d (ignored)\n", key, line);
    }
}

bool tess_config_load(tess_config *cfg, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return false;   // absent file is fine: defaults stand.
    }

    char buf[256];
    int line = 0;
    while (fgets(buf, sizeof buf, f)) {
        line++;
        // Strip an inline comment, then split on the first '='.
        char *hash = strchr(buf, '#');
        if (hash) {
            *hash = '\0';
        }
        char *eq = strchr(buf, '=');
        if (!eq) {
            char *only = trim(buf);
            if (*only) {
                fprintf(stderr, "config: no '=' on line %d (ignored)\n", line);
            }
            continue;   // blank/comment-only lines are fine.
        }
        *eq = '\0';
        char *key = trim(buf);
        char *val = trim(eq + 1);
        if (*key) {
            apply_kv(cfg, key, val, line);
        }
    }

    fclose(f);
    return true;
}
