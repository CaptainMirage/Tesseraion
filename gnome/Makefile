# Tesseraion -- GNOME wallpaper host build.
#
# Self-contained: compiles the render core from ../src/core UNCHANGED, generates the
# xdg-shell client protocol stubs with wayland-scanner, and links a raw
# wayland-client + EGL host. Nothing under ../src, ../shaders, or the root Makefile is
# touched. The core is reused only through its documented C API.
#
# Targets:
#   make          release build (-O2), warnings are errors, produces ./tesseraion-gnome
#   make run      build, then launch the host from the repo root (so shaders/ resolve)
#   make debug    -O0 -g build (still -Werror) for debugging
#   make install  build, then stage the extension + binary + shaders + a default
#                 config into the user's GNOME Shell extensions directory
#   make uninstall remove the installed extension directory
#   make enable   enable the installed extension in the live session (loads now)
#   make disable  disable it again (reverts all overrides, kills the host)
#   make logs     follow this extension's + host's journal output
#   make nested   try a nested GNOME Shell (needs the mutter-devkit viewer; see note)
#   make clean    remove build artifacts, generated protocol, and the binary
#
# Run note: the core loads shaders via CWD-relative paths ("shaders/..."), so the
# binary must run from a directory that contains shaders/. `make run` runs from the
# repo root; `make install` copies shaders/ next to the binary and the extension sets
# its working directory there.

BIN       := tesseraion-gnome
BUILD_DIR := build
PROTO_DIR := protocol

# Extension install location. UUID must match gnome/extension/metadata.json.
UUID      := tesseraion@captainmirage.github.io
EXT_DIR   := $(HOME)/.local/share/gnome-shell/extensions/$(UUID)

CC        := cc
CSTD      := -std=c11
WARN      := -Wall -Wextra -Werror
OPT       := -O2
INCLUDES  := -I../src -I../third_party -I$(PROTO_DIR)

# wayland-client (surface), wayland-egl (the drawable), egl (context), glesv2 (the
# ES3 entry points the core uses). Every dep is justified: this is the minimum to put
# a GLES 3.0 context on a Wayland surface.
PKGS       := wayland-client wayland-egl egl glesv2
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS   := $(shell pkg-config --libs $(PKGS))
LIBS       := $(PKG_LIBS) -lm

CFLAGS    := $(CSTD) $(WARN) $(OPT) $(INCLUDES) $(PKG_CFLAGS)
# Generated protocol code and the third-party stb impl are not part of our -Werror
# project surface; build them with warnings off so they never break the strict build.
CFLAGS_RELAX := $(CSTD) $(OPT) -w $(INCLUDES) $(PKG_CFLAGS)

# xdg-shell protocol XML ships with wayland-protocols; locate it via pkg-config.
WP_DIR  := $(shell pkg-config --variable=pkgdatadir wayland-protocols)
XDG_XML := $(WP_DIR)/stable/xdg-shell/xdg-shell.xml
PROTO_H := $(PROTO_DIR)/xdg-shell-client-protocol.h
PROTO_C := $(PROTO_DIR)/xdg-shell-protocol.c

# Strict sources: the render core (untouched) plus our host. Compiled with -Werror.
CORE_SRCS := $(wildcard ../src/core/*.c)
HOST_SRCS := $(wildcard src/*.c)
STRICT_OBJS := $(addprefix $(BUILD_DIR)/,$(notdir $(CORE_SRCS:.c=.o) $(HOST_SRCS:.c=.o)))

# Relaxed sources: the stb_truetype impl TU and the generated protocol code.
RELAX_OBJS := $(BUILD_DIR)/stb_impl.o $(BUILD_DIR)/xdg-shell-protocol.o

OBJS := $(STRICT_OBJS) $(RELAX_OBJS)
DEPS := $(STRICT_OBJS:.o=.d)

# VPATH lets the flat build/ object names resolve back to sources in their real dirs.
VPATH := ../src/core:src:../third_party:$(PROTO_DIR)

.PHONY: all run debug install uninstall enable disable logs nested clean
.DEFAULT_GOAL := all

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LIBS)

# Strict TUs. The order-only dep on the generated header guarantees it exists before
# the host TU (which includes it) is compiled.
$(STRICT_OBJS): $(BUILD_DIR)/%.o: %.c | $(PROTO_H)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Relaxed TUs (third-party + generated): warnings suppressed via target-specific
# CFLAGS so the shared pattern rule below can serve them too.
$(RELAX_OBJS): CFLAGS := $(CFLAGS_RELAX)
$(RELAX_OBJS): $(BUILD_DIR)/%.o: %.c | $(PROTO_H)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Generate the xdg-shell client stubs from the system protocol XML.
$(PROTO_H): $(XDG_XML) | $(PROTO_DIR)
	wayland-scanner client-header $< $@
$(PROTO_C): $(XDG_XML) | $(PROTO_DIR)
	wayland-scanner private-code $< $@
$(PROTO_DIR):
	@mkdir -p $@

run: $(BIN)
	cd .. && ./gnome/$(BIN)

debug: OPT := -O0 -g
debug: clean $(BIN)

# Stage everything the extension needs into one self-contained directory: the JS +
# metadata, the host binary, the shaders the core loads at runtime, and a default
# config (from the repo's example). The extension runs the binary with its working
# directory set here, so shaders/ and tesseraion.conf resolve next to it.
install: $(BIN)
	@mkdir -p $(EXT_DIR)
	cp extension/*.js extension/metadata.json $(EXT_DIR)/
	cp $(BIN) $(EXT_DIR)/
	rm -rf $(EXT_DIR)/shaders
	cp -r ../shaders $(EXT_DIR)/shaders
	cp ../tesseraion.conf.example $(EXT_DIR)/tesseraion.conf
	@echo "installed to $(EXT_DIR)"
	@echo "enable with: gnome-extensions enable $(UUID)  (after a shell restart / re-login, or in a nested shell)"

uninstall:
	rm -rf $(EXT_DIR)
	@echo "removed $(EXT_DIR)"

# Live-session control. On Wayland the shell cannot be restarted, so enabling loads
# the extension immediately and disabling reverts it immediately (InjectionManager
# restores every patched method and the host process is killed). This is the
# practical test path on builds without the nested devkit viewer.
enable: install
	gnome-extensions enable $(UUID)
	@echo "enabled; watch with 'make logs', revert with 'make disable'"

disable:
	-gnome-extensions disable $(UUID)
	@echo "disabled"

logs:
	journalctl --user -f -o cat | grep --line-buffered -iE "tesseraion|$(UUID)"

# Launch a nested GNOME Shell in its own Wayland session for safe iteration. We point
# the nested shell at a throwaway XDG_CONFIG_HOME so its dconf (incl. the
# enabled-extensions list) is isolated from the live desktop, then pre-enable this
# extension there. The extension itself lives in the shared XDG_DATA_HOME, so the
# nested shell still finds it. Close the nested window to end the session; the temp
# config is removed afterwards. Your real session is never touched.
#
# Shell 50 dropped --nested (and plain --wayland / standalone mutter take the native
# KMS backend, failing with EBUSY against the live seat). The replacement nested-dev
# mode is `gnome-shell --devkit`, but it runs a headless server plus a separate viewer
# binary (/usr/lib/mutter-devkit) that is NOT packaged on Arch/EndeavourOS, so no
# window appears here. Kept for systems that do ship the viewer; otherwise use
# `make enable` / `make disable` to test in the live session.
nested: install
	@tmp=$$(mktemp -d /tmp/tess-nested.XXXXXX); \
	echo "nested config home: $$tmp"; \
	XDG_CONFIG_HOME=$$tmp dbus-run-session -- sh -c "\
	  gsettings set org.gnome.shell disable-user-extensions false; \
	  gsettings set org.gnome.shell enabled-extensions \"['$(UUID)']\"; \
	  exec gnome-shell --devkit"; \
	rm -rf $$tmp

clean:
	rm -rf $(BUILD_DIR) $(PROTO_DIR) $(BIN)

-include $(DEPS)
