// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
//
// background.js -- put the host's surface into the desktop background.
//
// GNOME has no layer-shell, so the desktop background is a Meta.BackgroundActor
// owned by the shell. We override BackgroundManager._createBackgroundActor so that
// every background actor the shell builds (one per monitor) gets a child widget
// holding a Clutter.Clone of our host window. The clone paints the host's live
// surface below all windows; cloning also drives the host's frame callbacks, so the
// animation keeps advancing even though the real window is minimized.
//
// Override + reload-backgrounds technique adapted from the Hanabi extension.

import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import GLib from 'gi://GLib';
import Meta from 'gi://Meta';
import St from 'gi://St';

import {InjectionManager} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Background from 'resource:///org/gnome/shell/ui/background.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

/// A widget parented into a background actor that mirrors the host window for that
/// actor's monitor. It clones the matching host surface and fills the monitor.
const LiveWallpaper = GObject.registerClass(
    class LiveWallpaper extends St.Widget {
        constructor(backgroundActor, marker) {
            super({
                layout_manager: new Clutter.BinLayout(),
                width: backgroundActor.width,
                height: backgroundActor.height,
                x_expand: true,
                y_expand: true,
            });
            this._backgroundActor = backgroundActor;
            this._monitorIndex = backgroundActor.monitor;
            this._marker = marker;
            this._clone = null;
            this._timeoutId = 0;
            this._sourceDestroyId = 0;

            this.connect('destroy', () => this._onDestroy());

            // BinLayout lets the clone fill us regardless of the actor's own manager.
            backgroundActor.layout_manager = new Clutter.BinLayout();
            backgroundActor.add_child(this);

            this._apply();
        }

        /// Find the host window actor for this monitor. Bypass our own
        /// get_window_actors filter (false) so the hidden host is still visible here.
        _findSource() {
            const actors = global.get_window_actors(false);
            const ours = actors.filter(a => a.meta_window?.title?.includes(this._marker));
            return ours.find(a => a.meta_window.get_monitor() === this._monitorIndex)
                ?? ours[0] ?? null;
        }

        /// Attach the clone once the host window exists; retry on a timer until it
        /// does (the shell may build backgrounds before the host has mapped).
        _apply() {
            const tryAttach = () => {
                if (this._isDestroyed())
                    return GLib.SOURCE_REMOVE;
                const source = this._findSource();
                if (!source)
                    return GLib.SOURCE_CONTINUE;

                this._clone = new Clutter.Clone({source, x_expand: true, y_expand: true});
                this.add_child(this._clone);

                // If the host window goes away (process exit / restart), drop the dead
                // clone and resume polling for a fresh surface.
                this._sourceDestroyId = source.connect('destroy', () => {
                    this._sourceDestroyId = 0;
                    if (this._clone) {
                        this._clone.destroy();
                        this._clone = null;
                    }
                    if (!this._isDestroyed())
                        this._apply();
                });
                this._timeoutId = 0;
                return GLib.SOURCE_REMOVE;
            };

            if (tryAttach() === GLib.SOURCE_CONTINUE) {
                this._timeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 500, tryAttach);
            }
        }

        _isDestroyed() {
            return this._backgroundActor === null;
        }

        _onDestroy() {
            if (this._timeoutId) {
                GLib.Source.remove(this._timeoutId);
                this._timeoutId = 0;
            }
            if (this._clone) {
                if (this._sourceDestroyId && this._clone.source)
                    this._clone.source.disconnect(this._sourceDestroyId);
                this._sourceDestroyId = 0;
                this._clone.destroy();
                this._clone = null;
            }
            this._backgroundActor = null;
        }
    }
);

/// Installs the BackgroundManager override and tracks the wallpaper widgets so they
/// can be torn down cleanly. Recreates the shell's backgrounds on enable/disable so
/// our clones attach/detach without a logout.
export class BackgroundOverride {
    constructor(marker) {
        this._marker = marker;
        this._injection = new InjectionManager();
        this._widgets = new Set();
    }

    enable() {
        const self = this;
        this._injection.overrideMethod(
            Background.BackgroundManager.prototype, '_createBackgroundActor',
            originalMethod => function () {
                const backgroundActor = originalMethod.call(this);
                const widget = new LiveWallpaper(backgroundActor, self._marker);
                self._widgets.add(widget);
                widget.connect('destroy', () => self._widgets.delete(widget));
                return backgroundActor;
            });
        this._reloadBackgrounds();
    }

    disable() {
        this._injection.clear();
        for (const widget of [...this._widgets])
            widget.destroy();
        this._widgets.clear();
        this._reloadBackgrounds();
    }

    /// Ask the shell to rebuild its background actors so the override takes (or, on
    /// disable, the originals come back). Deferred to a "later" so we are not
    /// re-entrant inside a paint.
    _reloadBackgrounds() {
        const laters = global.compositor?.get_laters?.() ?? Meta.Laters?.get?.();
        laters?.add(Meta.LaterType.BEFORE_REDRAW, () => {
            Main.layoutManager._updateBackgrounds();
            return GLib.SOURCE_REMOVE;
        });
    }
}
