// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
// wl_host.h -- the GNOME wallpaper host interface.
//
// This host is "just another host" behind the render core's four-call API
// (renderer.h): it creates a raw wayland-client surface plus an EGL OpenGL ES 3.0
// context, sets a stable app_id so the companion GNOME Shell extension can match the
// window and move it into the desktop background, then drives the core. No GL,
// shader, atlas, or uniform handling lives here; that all stays in the core.

#ifndef TESS_WL_HOST_H
#define TESS_WL_HOST_H

#include "core/config.h"

/// The app_id this host advertises on its Wayland toplevel. The GNOME Shell
/// extension matches on exactly this string to find the wallpaper window.
#define TESS_GNOME_APP_ID "tesseraion-wallpaper"

/// Create the Wayland surface + GLES 3.0 context, run the render loop until the
/// compositor closes the window (or a fatal error), then tear everything down.
/// Returns 0 on a clean exit, nonzero on a setup failure (already logged).
///
/// `config_path` (may be NULL) is watched for live edits exactly as the dev host
/// does: when it changes on disk it is re-parsed over the defaults and applied to the
/// running effect, and the shader source files hot-reload the same way.
int tess_gnome_host_run(const tess_config *cfg, const char *config_path);

#endif // TESS_WL_HOST_H
