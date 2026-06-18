// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
//
// lifecycle.js -- pause the wallpaper when it cannot be seen.
//
// An idle wallpaper should not cook the GPU. The host already idles whenever the
// compositor stops sending it frame callbacks, and the compositor stops painting our
// clone (hence stops the callbacks) the moment the clone is hidden. So pausing is
// just hiding the clone for a monitor, which background.js handles; this module only
// decides WHEN. Two deterministic triggers:
//
//   - session locked: pause every monitor (the lock screen covers the desktop), and
//   - a monitor running a fullscreen window (a game or a video): pause that monitor.
//
// A monitor obscured by a normal/maximized window is left to Mutter's own occlusion
// culling (a fully covered background stops painting, so the host idles for free);
// only the deterministic cases are forced here. Monitor power-off (DPMS) likewise
// stops all painting on its own.

import Gio from 'gi://Gio';

// org.gnome.ScreenSaver.ActiveChanged is the stable, version-independent lock signal
// (true on lock, false on unlock), so we watch it rather than shell-internal state.
const SCREENSAVER_NAME = 'org.gnome.ScreenSaver';
const SCREENSAVER_PATH = '/org/gnome/ScreenSaver';

/// Watches lock and fullscreen state and tells the background override when to pause.
export class Lifecycle {
    constructor(background) {
        this._background = background;
        this._fullscreenId = 0;
        this._proxy = null;
        this._proxySignalId = 0;
        this._cancellable = null;
    }

    enable() {
        // A monitor entering or leaving fullscreen flips that monitor's pause state.
        this._fullscreenId = global.display.connect('in-fullscreen-changed',
            () => this._background.refreshPause());

        // Lock state over D-Bus. Built async so enable() never blocks; once the proxy
        // is up we also sync the current state in case we enabled while already locked.
        this._cancellable = new Gio.Cancellable();
        Gio.DBusProxy.new(
            Gio.DBus.session, Gio.DBusProxyFlags.NONE, null,
            SCREENSAVER_NAME, SCREENSAVER_PATH, SCREENSAVER_NAME,
            this._cancellable, (_source, res) => {
                let proxy;
                try {
                    proxy = Gio.DBusProxy.new_finish(res);
                } catch (e) {
                    if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                        logError(e, 'Tesseraion: ScreenSaver proxy init failed');
                    return;
                }
                this._proxy = proxy;
                this._proxySignalId = proxy.connectSignal('ActiveChanged',
                    (_p, _sender, [active]) => this._background.setLocked(active));
                // Sync the current lock state (GetActive returns one boolean).
                proxy.call('GetActive', null, Gio.DBusCallFlags.NONE, -1,
                    this._cancellable, (p, r) => {
                        try {
                            const [active] = p.call_finish(r).deepUnpack();
                            this._background.setLocked(active);
                        } catch (e) {
                            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                                logError(e, 'Tesseraion: ScreenSaver GetActive failed');
                        }
                    });
            });
    }

    disable() {
        if (this._fullscreenId) {
            global.display.disconnect(this._fullscreenId);
            this._fullscreenId = 0;
        }
        if (this._cancellable) {
            this._cancellable.cancel();
            this._cancellable = null;
        }
        if (this._proxy && this._proxySignalId) {
            this._proxy.disconnectSignal(this._proxySignalId);
            this._proxySignalId = 0;
        }
        this._proxy = null;
    }
}
