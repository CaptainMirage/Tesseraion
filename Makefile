# Tesseraion -- lean build for the render core + GLFW dev host.
#
# Targets:
#   make        release build (-O2), warnings are errors, produces ./tesseraion
#   make run    build, then launch the GLFW dev host
#   make debug  -O0 -g build (still -Werror) for debugging
#   make clean  remove build artifacts and the binary
#
# Dependencies are resolved through pkg-config (glfw3 for the window/context,
# glesv2 for the OpenGL ES 3.0 entry points). On Mesa we link libGLESv2
# directly, so no GL loader (glad/glew) is needed.

BIN       := tesseraion
BUILD_DIR := build

CC        := cc
CSTD      := -std=c11
WARN      := -Wall -Wextra -Werror
OPT       := -O2
DEFS      :=
INCLUDES  := -Isrc -Ithird_party

PKGS      := glfw3 glesv2
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS   := $(shell pkg-config --libs $(PKGS))

# libm for the noise math the shader host may need; harmless if unused.
LIBS      := $(PKG_LIBS) -lm

CFLAGS    := $(CSTD) $(WARN) $(OPT) $(DEFS) $(INCLUDES) $(PKG_CFLAGS)
# Third-party single-header impls are not warning-clean; build them with warnings
# off so they never break our -Werror project build.
CFLAGS_TP := $(CSTD) $(OPT) -w $(DEFS) $(INCLUDES) $(PKG_CFLAGS)

# Our sources (strict) plus third-party impl TUs (relaxed).
SRCS      := $(shell find src -name '*.c')
TP_SRCS   := $(shell find third_party -name '*.c')
OBJS      := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
TP_OBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(TP_SRCS))
DEPS      := $(OBJS:.o=.d) $(TP_OBJS:.o=.d)

.PHONY: all run debug clean

all: $(BIN)

$(BIN): $(OBJS) $(TP_OBJS)
	$(CC) $(OBJS) $(TP_OBJS) -o $@ $(LIBS)

# Compile each .c, mirroring the source tree under build/. -MMD -MP emits header
# dependency files so edits to headers trigger the right rebuilds.
$(BUILD_DIR)/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Third-party TUs: same build, warnings suppressed.
$(BUILD_DIR)/third_party/%.o: third_party/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_TP) -MMD -MP -c $< -o $@

run: $(BIN)
	./$(BIN)

debug: OPT := -O0 -g
debug: clean $(BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN)

-include $(DEPS)
