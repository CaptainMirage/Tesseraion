// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
//
// shellHide.js -- keep the host window out of every shell surface.
//
// The host is a real toplevel, so without this it would appear in the overview,
// the alt-tab switcher, the window list, and the dash. We patch the shell methods
// that enumerate windows to drop any whose title carries our marker. Title match
// (not pid) is deliberate: a Meta.WaylandClient child inherits gnome-shell's pid,
// so pid-based filters would misfire. Set of patched methods follows the Hanabi
// extension, which solved the same identification problem.

import Meta from 'gi://Meta';
import Shell from 'gi://Shell';

import {InjectionManager} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Workspace from 'resource:///org/gnome/shell/ui/workspace.js';
import * as WorkspaceThumbnail from 'resource:///org/gnome/shell/ui/workspaceThumbnail.js';

export class ShellHide {
    constructor(marker) {
        this._marker = marker;
        this._injection = new InjectionManager();
    }

    /// True if this MetaWindow is our hidden host surface.
    _isOurs(window) {
        return !!window?.title?.includes(this._marker);
    }

    enable() {
        const self = this;
        const isOurs = w => self._isOurs(w);

        // Window-actor list: filtered by default; pass false to bypass (background.js
        // needs the real list to locate the surface it clones).
        this._injection.overrideMethod(
            Shell.Global.prototype, 'get_window_actors',
            originalMethod => function (hide = true) {
                const actors = originalMethod.call(this);
                return hide ? actors.filter(a => !isOurs(a.meta_window)) : actors;
            });

        // Overview previews (main view + workspace thumbnails).
        for (const proto of [Workspace.Workspace.prototype,
            WorkspaceThumbnail.WorkspaceThumbnail.prototype]) {
            this._injection.overrideMethod(proto, '_isOverviewWindow',
                originalMethod => function (window) {
                    return isOurs(window) ? false : originalMethod.call(this, window);
                });
        }

        // Alt-tab / ctrl-alt-tab tab lists.
        this._injection.overrideMethod(
            Meta.Display.prototype, 'get_tab_list',
            originalMethod => function (type, workspace) {
                return originalMethod.call(this, type, workspace)
                    .filter(w => !isOurs(w));
            });

        // App tracking: keep the host out of the dash and running-apps. Also detaches
        // it from any real app that happens to share the inherited shell pid.
        this._injection.overrideMethod(
            Shell.WindowTracker.prototype, 'get_window_app',
            originalMethod => function (window) {
                return isOurs(window) ? null : originalMethod.call(this, window);
            });
        this._injection.overrideMethod(
            Shell.App.prototype, 'get_windows',
            originalMethod => function () {
                return originalMethod.call(this).filter(w => !isOurs(w));
            });
        this._injection.overrideMethod(
            Shell.App.prototype, 'get_n_windows',
            _originalMethod => function () {
                return this.get_windows().length;
            });
        this._injection.overrideMethod(
            Shell.AppSystem.prototype, 'get_running',
            originalMethod => function () {
                return originalMethod.call(this).filter(app => app.get_n_windows() > 0);
            });
    }

    disable() {
        this._injection.clear();
    }
}
