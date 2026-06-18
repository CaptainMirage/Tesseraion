# Testing it safely in a throwaway account

This extension reaches into GNOME Shell internals (it clones a window into the
desktop background and patches a few Shell methods to hide that window). When
you are developing it, you do not want a half-finished version disrupting your
real desktop. This guide sets up an isolated GNOME session to test in, without
ever logging out of your own.

## Why a separate user account

Two facts force this approach on a modern GNOME (Shell 50, Wayland):

- There is no working "nested GNOME Shell in a window" here. Mutter 50 dropped the
  old `--nested` mode; its replacement (`gnome-shell --devkit`) needs a separate
  viewer binary (`/usr/lib/mutter-devkit`) that is not packaged on Arch /
  EndeavourOS, so no window appears.
- GNOME's "Remote Login" (headless) gives a fresh, isolated session you can view
  in a window, but it refuses to start a second session for a user who already has
  one running. Logging in remotely as yourself would force-stop your real session.

The fix is to log in remotely as a different, throwaway user. A fresh user has no
running session, so Remote Login works, and its desktop and settings are fully
isolated from yours.

We call that user `tessra-dev-test` below.

## One-time setup

Install a remote-desktop daemon and a viewer client:

```bash
sudo pacman -S gnome-remote-desktop gnome-connections
```

Create the throwaway user and give it a password:

```bash
sudo useradd -m tessra-dev-test
sudo passwd tessra-dev-test
```

Turn on Remote Login: open **Settings -> System -> Remote Desktop**, switch to
the **Remote Login** tab (not "Desktop Sharing", which only mirrors your current
screen), enable it, and set a username and password there. That panel generates
the TLS certificate and starts the service for you.

## Each test run

1. Build the host as yourself (no root needed):

   ```bash
   cd gnome
   make
   ```

2. Assemble the extension straight into the test user's home. This copies the
   freshly built pieces so nothing is staged on your own account:

   ```bash
   DEST="/home/tessra-dev-test/.local/share/gnome-shell/extensions/tesseraion@captainmirage.github.io"
   sudo rm -rf "$DEST"
   sudo mkdir -p "$DEST"
   sudo cp extension/*.js extension/metadata.json "$DEST/"
   sudo cp tesseraion-gnome "$DEST/"
   sudo cp -r ../shaders "$DEST/shaders"
   sudo cp ../tesseraion.conf.example "$DEST/tesseraion.conf"
   sudo chown -R tessra-dev-test:tessra-dev-test "$DEST"
   ```

3. Connect to the isolated session. Open **Connections**, add `localhost`, and
   log in. You first pass the Remote Login credentials you set during setup, then
   a GDM login appears in the window: log in there as `tessra-dev-test`. Because
   that user has no other session, nothing of yours is touched.

4. Inside that session, open a terminal and enable the extension:

   ```bash
   gnome-extensions enable tesseraion@captainmirage.github.io
   ```

   The wallpaper should appear immediately (a fresh session has already scanned
   the extensions folder, so no re-login is needed there).

## Cleanup

When you are done testing for good:

```bash
sudo userdel -r tessra-dev-test
```

And, if you do not want Remote Login left on, turn it back off in
**Settings -> System -> Remote Desktop**.
