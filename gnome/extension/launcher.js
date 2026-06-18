// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Captain Mirage
//
// launcher.js -- spawn the Tesseraion host as a shell-owned Wayland client.
//
// Using Meta.WaylandClient (rather than a plain subprocess) ties the host's
// lifetime to the extension and, crucially, lets us ask the compositor which
// windows belong to us via owns_window(), the precise way to claim our surface
// the moment it maps. The technique follows the DING / Hanabi extensions.

import Meta from 'gi://Meta';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import * as Config from 'resource:///org/gnome/shell/misc/config.js';

const shellVersion = parseInt(Config.PACKAGE_VERSION.split('.')[0]);

/// Owns the host subprocess and the Meta.WaylandClient wrapping it.
export class Launcher {
    constructor() {
        this._launcher = new Gio.SubprocessLauncher({
            flags: Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_MERGE,
        });
        this._waylandClient = null;
        this._subprocess = null;
        this._cancellable = new Gio.Cancellable();
        this._stdout = null;
        this.running = false;
    }

    /// Run the host with this working directory so the core resolves its
    /// CWD-relative shader and config paths from the install location.
    set_cwd(cwd) {
        this._launcher.set_cwd(cwd);
    }

    /// Spawn argv as a Wayland client owned by the shell. Returns the subprocess
    /// or null. Shell 49+ creates the client straight from argv; older builds
    /// construct the client first then spawnv (kept for forward/backward safety).
    spawnv(argv) {
        if (shellVersion >= 49) {
            this._waylandClient = Meta.WaylandClient.new_subprocess(
                global.context, this._launcher, argv);
            this._subprocess = this._waylandClient.get_subprocess();
        } else {
            this._waylandClient = Meta.WaylandClient.new(global.context, this._launcher);
            this._subprocess = this._waylandClient.spawnv(global.display, argv);
        }

        // The launcher is single-use; release it once the child is spawned.
        if (this._launcher.close)
            this._launcher.close();
        this._launcher = null;

        if (this._subprocess) {
            this.running = true;
            this._pipeOutput();
            this._subprocess.wait_async(this._cancellable, () => {
                this.running = false;
            });
        }
        return this._subprocess;
    }

    /// True if metaWindow is one of our host's surfaces.
    owns_window(metaWindow) {
        if (!this.running || !this._waylandClient)
            return false;
        try {
            return this._waylandClient.owns_window(metaWindow);
        } catch {
            return false;
        }
    }

    /// Forward the host's stdout/stderr to the journal so renderer errors surface
    /// under `journalctl --user -f` alongside the shell log. Reads line by line.
    _pipeOutput() {
        this._stdout = Gio.DataInputStream.new(this._subprocess.get_stdout_pipe());
        const readNext = () => {
            if (!this._stdout)
                return;
            this._stdout.read_line_async(GLib.PRIORITY_DEFAULT, this._cancellable,
                (stream, res) => {
                    try {
                        const [line, len] = stream.read_line_finish_utf8(res);
                        if (len)
                            console.log(`[tesseraion] ${line}`);
                    } catch (e) {
                        if (e.matches?.(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                            return;
                        return;
                    }
                    readNext();
                });
        };
        readNext();
    }

    /// Stop the host and drop our references. Safe to call more than once.
    stop() {
        this._cancellable.cancel();
        if (this._subprocess && this.running) {
            try {
                this._subprocess.force_exit();
            } catch {
                // Already gone; nothing to do.
            }
        }
        this.running = false;
        this._stdout = null;
        this._subprocess = null;
        this._waylandClient = null;
    }
}
