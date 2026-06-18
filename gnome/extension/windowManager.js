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

        this.sizeToTarget();
        this._window.lower();
        this._window.minimize();
    }

    /// Resize the host window to the largest connected monitor so the highest-
    /// resolution output is rendered natively and every other monitor's clone is a
    /// clean downscale (a single host is cloned onto all monitors; per-monitor
    /// independent fields are a possible later addition). The host receives the
    /// configure and resizes its GLES surface to match.
    ///
    /// Sizes in logical pixels: at scale 1.0 (the common case) that is the native
    /// resolution. Crisp rendering under fractional/HiDPI scaling would need the host
    /// to speak the fractional-scale + viewporter protocols and is deferred.
    sizeToTarget() {
        if (this._disposed)
            return;
        const monitors = Main.layoutManager.monitors;
        if (!monitors || monitors.length === 0)
            return;
        // Pick the largest by pixel area; fall back to the primary on a tie/!found.
        let target = monitors[Main.layoutManager.primaryIndex] ?? monitors[0];
        let bestArea = target ? target.width * target.height : 0;
        for (const mon of monitors) {
            const area = mon.width * mon.height;
            if (area > bestArea) {
                target = mon;
                bestArea = area;
            }
        }
        if (!target)
            return;
        // move_resize_frame(userOp=false, x, y, w, h): a programmatic, non-user move.
        this._window.move_resize_frame(false, target.x, target.y,
                                       target.width, target.height);
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
        this._monitorsChangedId = 0;
        this._resizeIdleId = 0;
    }

    enable() {
        this._mapId = global.window_manager.connect_after('map', (_wm, actor) => {
            const window = actor.get_meta_window();
            if (window && this._launcher.owns_window(window))
                this._add(window);
        });
        // On a resolution change or monitor hotplug (e.g. plugging in a TV), the shell
        // rebuilds its background actors (so our clones re-attach automatically); we
        // just need to re-size the host to the new largest monitor. Defer one idle tick
        // so the new monitor geometry has settled before we read it.
        this._monitorsChangedId = Main.layoutManager.connect('monitors-changed',
            () => this._onMonitorsChanged());
    }

    _onMonitorsChanged() {
        if (this._resizeIdleId)
            return;   // coalesce a burst of changes into one resize.
        this._resizeIdleId = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            this._resizeIdleId = 0;
            for (const {mw} of this._managed.values())
                mw.sizeToTarget();
            return GLib.SOURCE_REMOVE;
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
        if (this._monitorsChangedId) {
            Main.layoutManager.disconnect(this._monitorsChangedId);
            this._monitorsChangedId = 0;
        }
        if (this._resizeIdleId) {
            GLib.Source.remove(this._resizeIdleId);
            this._resizeIdleId = 0;
        }
        for (const window of [...this._managed.keys()])
            this._remove(window);
        this._managed.clear();
    }
}
