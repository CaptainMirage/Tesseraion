// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
// main.c -- GNOME wallpaper host entry point: build a config, hand it to the host.
//
// Deliberately thin, mirroring the dev host's main: resolve a config file
// (argv[1], else "tesseraion.conf"), overlay it on the defaults, pick a seed, and
// launch the raw wayland + EGL host. The render core is reused unchanged through its
// documented C API; nothing in src/core is touched.

#include "core/config.h"
#include "wl_host.h"

#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv) {
    const char *config_path = (argc > 1) ? argv[1] : "tesseraion.conf";

    tess_config cfg;
    tess_config_default(&cfg);
    tess_config_load(&cfg, config_path);   // absent file is fine; defaults stand.

    // Unless the config pins a seed, randomize the pattern offset so each launch
    // looks different (mirrors the dev host and the website's per-load random start).
    if (!cfg.seed_pinned) {
        srand((unsigned)time(NULL));
        cfg.seed_x = (float)rand() / (float)RAND_MAX * 1000.0f;
        cfg.seed_y = (float)rand() / (float)RAND_MAX * 1000.0f;
    }

    return tess_gnome_host_run(&cfg, config_path);
}
