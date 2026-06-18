// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
//
// extension.js -- Tesseraion GNOME wallpaper, orchestration.
//
// Wires the pieces together: install the window-hiding overrides and the
// background clone override first, then spawn the GLES host as a shell-owned
// Wayland client. The host binary, its shaders, and a default config are installed
// alongside this file, and the host runs with its working directory set here so the
// core resolves those CWD-relative paths.

import GLib from 'gi://GLib';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

import {Launcher} from './launcher.js';
import {WindowManager} from './windowManager.js';
import {ShellHide} from './shellHide.js';
import {BackgroundOverride} from './background.js';

// Unique window-title token the host sets and every match here keys on. Keep in
// sync with WINDOW_TITLE in gnome/src/wl_host.c.
const WINDOW_MARKER = 'tesseraion-wallpaper';
const HOST_BINARY = 'tesseraion-gnome';
const CONFIG_FILE = 'tesseraion.conf';

export default class TesseraionWallpaperExtension extends Extension {
    enable() {
        const dir = this.path;
        const binary = GLib.build_filenamev([dir, HOST_BINARY]);
        const config = GLib.build_filenamev([dir, CONFIG_FILE]);

        // Install the shell-side hooks before spawning so the window is hidden and
        // claimed the instant it maps (no flash of a normal window).
        this._shellHide = new ShellHide(WINDOW_MARKER);
        this._shellHide.enable();

        this._windowManager = new WindowManager(this._launcher = new Launcher());
        this._windowManager.enable();

        this._background = new BackgroundOverride(WINDOW_MARKER);
        this._background.enable();

        // CWD = the install dir so the core finds shaders/ and the config there.
        this._launcher.set_cwd(dir);
        this._launcher.spawnv([binary, config]);
    }

    disable() {
        // Tear down in reverse: stop painting clones, restore the shell methods, then
        // kill the host. Each guard tolerates a partial enable().
        this._background?.disable();
        this._shellHide?.disable();
        this._windowManager?.disable();
        this._launcher?.stop();

        this._background = null;
        this._shellHide = null;
        this._windowManager = null;
        this._launcher = null;
    }
}
