// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
//
// windowManager.js -- tame the host's real toplevel.
//
// The host opens an ordinary Wayland toplevel; on its own that would float on the
// desktop and sit in the window list. We claim it on map (by Wayland-client
// ownership), size it to its monitor so the cloned surface fills the background,
// and minimize + keep-at-bottom it so the real window never shows. The background
// clone (see background.js) still paints the surface live, which keeps the
// compositor sending the host its frame callbacks even while minimized.
//
// Approach adapted from the ManageWindow helper in the DING / Hanabi extensions.

import GLib from 'gi://GLib';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

/// Per-window keeper: enforces minimized + bottom of the stack and a monitor-sized
/// frame, re-applying on the signals that would otherwise undo it.
class ManagedWindow {
    constructor(window) {
        this._window = window;
        this._signals = [];
        this._disposed = false;

        // Re-lower if something raises us; never let us be marked above.
        this._signals.push(window.connect_after('raised', () => {
            if (!this._disposed)
                this._window.lower();
        }));
        this._signals.push(window.connect('notify::above', () => {
            if (!this._disposed && this._window.above)
                this._window.unmake_above();
        }));
        // Re-minimize if the window gets unminimized (e.g. by another extension).
        this._signals.push(window.connect('notify::minimized', () => {
            if (!this._disposed && !this._window.minimized)
                this._window.minimize();
        }));

        this._sizeToMonitor();
        this._window.lower();
        this._window.minimize();
    }

    /// Resize the host window to fill its monitor so the 1:1 clone covers the
    /// background. The host receives the configure and resizes its GLES surface.
    /// Multi-monitor/HiDPI refinement is a later chunk; target the primary for now.
    _sizeToMonitor() {
        const idx = Main.layoutManager.primaryIndex;
        const mon = Main.layoutManager.monitors[idx];
        if (!mon)
            return;
        // move_resize_frame(userOp=false, x, y, w, h): a programmatic, non-user move.
        this._window.move_resize_frame(false, mon.x, mon.y, mon.width, mon.height);
    }

    destroy() {
        this._disposed = true;
        for (const id of this._signals)
            this._window.disconnect(id);
        this._signals = [];
        // Restore the window to a normal state for the brief moment before the host
        // process is killed on disable, so nothing is left wedged if teardown races.
        if (this._window.minimized)
            this._window.unminimize();
        this._window = null;
    }
}

/// Watches for the host's window mapping and manages every surface it owns.
export class WindowManager {
    constructor(launcher) {
        this._launcher = launcher;
        this._managed = new Map();   // MetaWindow -> { mw: ManagedWindow, unmanagedId }
        this._mapId = 0;
    }

    enable() {
        this._mapId = global.window_manager.connect_after('map', (_wm, actor) => {
            const window = actor.get_meta_window();
            if (window && this._launcher.owns_window(window))
                this._add(window);
        });
    }

    _add(window) {
        if (this._managed.has(window))
            return;
        // Defer one idle tick: at 'map' time the window may not yet report its final
        // monitor, and move_resize lands more reliably just after.
        GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            if (!window || this._managed.has(window))
                return GLib.SOURCE_REMOVE;
            const mw = new ManagedWindow(window);
            const unmanagedId = window.connect('unmanaged', w => this._remove(w));
            this._managed.set(window, {mw, unmanagedId});
            return GLib.SOURCE_REMOVE;
        });
    }

    _remove(window) {
        const entry = this._managed.get(window);
        if (!entry)
            return;
        window.disconnect(entry.unmanagedId);
        entry.mw.destroy();
        this._managed.delete(window);
    }

    disable() {
        if (this._mapId) {
            global.window_manager.disconnect(this._mapId);
            this._mapId = 0;
        }
        for (const window of [...this._managed.keys()])
            this._remove(window);
        this._managed.clear();
    }
}
