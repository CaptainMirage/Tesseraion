// config.c -- defaults for the render-core tunables (see config.h).

#include "core/config.h"

void tess_config_default(tess_config *cfg) {
    // Defaults from the original website background effect.
    cfg->fps_cap   = 30;
    cfg->speed     = 1.25;

    cfg->font_size    = 10.0f;
    cfg->char_w_ratio = 0.62f;
    cfg->line_h_ratio = 1.4f;
    cfg->max_cols     = 260;

    cfg->ramp = " .:-=+*#%@";

    cfg->noise_scale = 3.3f;
    cfg->warp        = 0.6f;
    cfg->softness    = 1.2f;
    cfg->skip        = 0.28f;
    cfg->alpha_cap   = 0.50f;

    cfg->mid_rgb[0]  = 120.0f; cfg->mid_rgb[1]  = 128.0f; cfg->mid_rgb[2]  = 148.0f;
    cfg->peak_rgb[0] =  74.0f; cfg->peak_rgb[1] = 140.0f; cfg->peak_rgb[2] = 255.0f;
    cfg->blue_start   = 0.60f;
    cfg->blue_full    = 0.93f;
    cfg->accent_boost = 0.30f;
}
