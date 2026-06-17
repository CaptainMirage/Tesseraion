// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
// config.h -- runtime tunables for the render core.
//
// One flat struct of every knob the effect exposes, modelled on the original
// website background effect so the dev host reproduces that look. The full set
// is defined up front so the struct stays a stable contract shared by the shader
// stages, the config file parser (CP4), and the future wallpaper host.
// tess_config_default() fills in the matching defaults.

#ifndef TESS_CONFIG_H
#define TESS_CONFIG_H

#include <stdbool.h>

/// Max ramp length we store inline (owns the string, so a config is safe to copy
/// and free of heap ownership).
#define TESS_RAMP_MAX 64

/// All effect + host tunables. Plain values, no ownership, safe to copy.
typedef struct {
    // --- Host / timing ---
    int    fps_cap;        ///< frame cap so an idle wallpaper does not cook the GPU.
    double speed;          ///< field-time advanced per real second (drift rate).

    // --- Cell metrics ---
    float  font_size;      ///< px the cell derives from (JS FONT_SIZE: 10).
    float  char_w_ratio;   ///< cell width  = font_size * this (JS 0.62).
    float  line_h_ratio;   ///< cell height = font_size * this (JS 1.4).
    int    max_cols;       ///< cap columns on wide screens; cell grows past this (JS 260).

    // --- Char ramp ---
    char   ramp[TESS_RAMP_MAX];  ///< sparse->dense glyphs (JS CHARS: " .:-=+*#%@").

    // --- Noise field ---
    float  noise_scale;    ///< feature size across the viewport (JS 3.3).
    float  warp;           ///< domain-warp amount (JS 0.6).
    float  softness;       ///< tanh contrast; effective = softness*2.2*2 (JS 1.2).
    float  skip;           ///< intensities below this draw nothing (JS 0.28).
    float  alpha_cap;      ///< base per-cell alpha scaler (JS 0.50).
    float  fade_band;      ///< width above skip to fade cells up from black.

    // --- Seed ---
    // Pattern offset into the infinite noise field. Pinned reproduces a fixed
    // pattern; unpinned is randomized per load so each launch looks different.
    float  seed_x;         ///< x offset into the field.
    float  seed_y;         ///< y offset into the field.
    bool   seed_pinned;    ///< true once a seed is set explicitly (config or pin).

    // --- Palette ---
    float  mid_rgb[3];     ///< gray base for mid intensities (JS [120,128,148], 0..255).
    float  peak_rgb[3];    ///< blue crest colour (JS [74,140,255], 0..255).
    float  blue_start;     ///< intensity where blue begins (JS 0.60).
    float  blue_full;      ///< intensity that reads fully blue (JS 0.93).
    float  accent_boost;   ///< extra alpha on blue crests (JS 0.30).
} tess_config;

/// Fill cfg with the defaults modelled on the original website background effect.
void tess_config_default(tess_config *cfg);

/// Overlay a `key = value` config file onto `cfg` (call after _default). Lines
/// are `key = value`, `#` starts a comment, blanks are ignored; unknown keys and
/// malformed values are warned about and skipped. A `seed` key pins the seed.
/// Returns true if the file was opened and parsed, false if it was absent (in
/// which case `cfg` keeps its current values).
bool tess_config_load(tess_config *cfg, const char *path);

#endif // TESS_CONFIG_H
