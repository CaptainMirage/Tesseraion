# Tesseraion -- GNOME wallpaper host build.
#
# Self-contained: compiles the render core from ../src/core UNCHANGED, generates the
# xdg-shell client protocol stubs with wayland-scanner, and links a raw
# wayland-client + EGL host. Nothing under ../src, ../shaders, or the root Makefile is
# touched. The core is reused only through its documented C API.
#
# Targets:
#   make        release build (-O2), warnings are errors, produces ./tesseraion-gnome
#   make run    build, then launch the host from the repo root (so shaders/ resolve)
#   make debug  -O0 -g build (still -Werror) for debugging
#   make clean  remove build artifacts, generated protocol, and the binary
#
# Run note: the core loads shaders via CWD-relative paths ("shaders/..."), so the
# binary must run from the repo root. `make run` does that for you.

BIN       := tesseraion-gnome
BUILD_DIR := build
PROTO_DIR := protocol

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

.PHONY: all run debug clean
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

clean:
	rm -rf $(BUILD_DIR) $(PROTO_DIR) $(BIN)

-include $(DEPS)
