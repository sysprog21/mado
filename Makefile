-include .config

# Set default goal explicitly
.DEFAULT_GOAL := all

check_goal := $(strip $(MAKECMDGOALS))
ifeq ($(filter $(check_goal),config defconfig),)
    ifneq "$(CONFIG_CONFIGURED)" "y"
        $(error You must first run 'make config' or 'make defconfig')
    endif
endif

# Target variables initialization

target-y :=
target.o-y :=
TARGET_LIBS :=

target.a-y :=
target.a-$(CONFIG_DEMO_APPLICATIONS) += libapps.a
target.a-y += libtwin.a

# Core library

libtwin.a_cflags-y :=

libtwin.a_files-y = \
	src/box.c \
	src/poly.c \
	src/toplevel.c \
	src/button.c \
	src/fixed.c \
	src/label.c \
	src/trig.c \
	src/convolve.c \
	src/font.c \
	src/matrix.c \
	src/queue.c \
	src/widget.c \
	src/font_default.c \
	src/path.c \
	src/screen.c \
	src/window.c \
	src/dispatch.c \
	src/geom.c \
	src/pattern.c \
	src/spline.c \
	src/work.c \
	src/draw-common.c \
	src/hull.c \
	src/icon.c \
	src/pixmap.c \
	src/timeout.c \
	src/image.c \
	src/animation.c \
	src/closure.c \
	src/api.c

libtwin.a_includes-y := \
	include \
	src

# Auto-generate compositing function declarations if missing
src/composite-decls.h: scripts/gen-composite-decls.py
	@echo "  GEN        $@"
	@$< > $@

# Optional features

libtwin.a_files-$(CONFIG_LOGGING) += src/log.c
libtwin.a_files-$(CONFIG_CURSOR) += src/cursor.c

# Rendering backends
# Screen compositing operations (always needed for screen buffer management)
libtwin.a_files-y += src/screen-ops.c
# Renderer implementations (draw-builtin.c includes all compositing operations)
libtwin.a_files-$(CONFIG_RENDERER_BUILTIN) += src/draw-builtin.c
libtwin.a_files-$(CONFIG_RENDERER_PIXMAN) += src/draw-pixman.c
libtwin.a_cflags-$(CONFIG_RENDERER_PIXMAN) += $(shell pkg-config --cflags pixman-1)
ifeq ($(CONFIG_RENDERER_PIXMAN), y)
TARGET_LIBS += $(shell pkg-config --libs pixman-1)
endif

# Image loaders

ifeq ($(CONFIG_LOADER_JPEG), y)
libtwin.a_files-y += src/image-jpeg.c
libtwin.a_cflags-y += $(shell pkg-config --cflags libjpeg)
TARGET_LIBS += $(shell pkg-config --libs libjpeg)
endif

ifeq ($(CONFIG_LOADER_PNG), y)
libtwin.a_files-y += src/image-png.c
libtwin.a_cflags-y += $(shell pkg-config --cflags libpng)
TARGET_LIBS += $(shell pkg-config --libs libpng)
endif

ifeq ($(CONFIG_LOADER_GIF), y)
libtwin.a_files-y += src/image-gif.c
endif

ifeq ($(CONFIG_LOADER_TVG), y)
libtwin.a_files-y += src/image-tvg.c
endif

# Applications

libapps.a_files-y := apps/dummy.c
libapps.a_files-$(CONFIG_DEMO_MULTI) += apps/multi.c
libapps.a_files-$(CONFIG_DEMO_HELLO) += apps/hello.c
libapps.a_files-$(CONFIG_DEMO_CLOCK) += apps/clock.c
libapps.a_files-$(CONFIG_DEMO_CALCULATOR) += apps/calc.c
libapps.a_files-$(CONFIG_DEMO_LINE) += apps/line.c
libapps.a_files-$(CONFIG_DEMO_SPLINE) += apps/spline.c
libapps.a_files-$(CONFIG_DEMO_ANIMATION) += apps/animation.c
libapps.a_files-$(CONFIG_DEMO_IMAGE) += apps/image.c

libapps.a_includes-y := include

# Graphical backends

BACKEND := none

ifeq ($(CONFIG_BACKEND_SDL), y)
BACKEND = sdl
libtwin.a_files-y += backend/sdl.c
libtwin.a_cflags-y += $(shell sdl2-config --cflags)
TARGET_LIBS += $(shell sdl2-config --libs)
endif

ifeq ($(CONFIG_BACKEND_FBDEV), y)
BACKEND = fbdev
TARGET_LIBS += -ludev -pthread
libtwin.a_files-y += backend/fbdev.c
libtwin.a_files-y += backend/linux_input.c
endif

ifeq ($(CONFIG_BACKEND_VNC), y)
BACKEND = vnc
libtwin.a_files-y += backend/vnc.c
libtwin.a_files-y += src/cursor.c
libtwin.a_cflags-y += $(shell pkg-config --cflags neatvnc aml pixman-1)
TARGET_LIBS += $(shell pkg-config --libs neatvnc aml pixman-1)
endif

ifeq ($(CONFIG_BACKEND_HEADLESS), y)
BACKEND = headless
libtwin.a_files-y += backend/headless.c
endif

# Performance tester

ifeq ($(CONFIG_PERF_TEST), y)
target-$(CONFIG_PERF_TEST) += mado-perf
mado-perf_depends-y += $(target.a-y)
mado-perf_files-y += tools/perf.c
mado-perf_includes-y := include
mado-perf_ldflags-y := \
	$(target.a-y) \
	$(TARGET_LIBS)
endif

# Standalone application

ifeq ($(CONFIG_DEMO_APPLICATIONS), y)
target-y += demo-$(BACKEND)
demo-$(BACKEND)_depends-y += $(target.a-y)
demo-$(BACKEND)_files-y = apps/main.c
demo-$(BACKEND)_includes-y := include
demo-$(BACKEND)_ldflags-y := \
	$(target.a-y) \
	$(TARGET_LIBS)

# Emscripten-specific linker flags to avoid "section too large" errors
ifeq ($(CC_IS_EMCC), 1)
demo-$(BACKEND)_ldflags-y += \
	-sINITIAL_MEMORY=33554432 \
	-sALLOW_MEMORY_GROWTH=1 \
	-sSTACK_SIZE=1048576
endif
endif

# Font editor tool

ifeq ($(CONFIG_TOOLS), y)
target-$(CONFIG_TOOL_FONTEDIT) += font-edit
font-edit_files-y = \
	tools/font-edit/sfit.c \
	tools/font-edit/font-edit.c
font-edit_includes-y := tools/font-edit
font-edit_cflags-y := \
	$(shell pkg-config --cflags cairo) \
	$(shell sdl2-config --cflags)
font-edit_ldflags-y := \
	$(shell pkg-config --libs cairo) \
	$(shell sdl2-config --libs) \
	-lm

# Headless control tool
target-$(CONFIG_TOOL_HEADLESS_CTL) += headless-ctl
headless-ctl_files-y = tools/headless-ctl.c
headless-ctl_includes-y := include
headless-ctl_ldflags-y := # -lrt
endif

# Build system integration

CFLAGS += -include config.h

# Ensure composite-decls.h exists before including build rules
# (needed for dependency generation in mk/common.mk)
ifeq ($(filter config defconfig clean,$(MAKECMDGOALS)),)
    ifeq ($(wildcard src/composite-decls.h),)
        $(shell scripts/gen-composite-decls.py > src/composite-decls.h)
    endif
endif

# Only skip build rules when running ONLY config/defconfig (no other targets)
ifneq ($(filter-out config defconfig,$(check_goal)),)
    # Has targets other than config/defconfig
    include mk/common.mk
else ifeq ($(check_goal),)
    # Empty goals means building 'all'
    include mk/common.mk
endif
# Otherwise, only config/defconfig targets - skip mk/common.mk

KCONFIGLIB := tools/kconfig/kconfiglib.py
$(KCONFIGLIB):
	@if [ -d .git ]; then \
	    git submodule update --init tools/kconfig; \
	else \
	    echo "Error: Kconfig tools not found"; \
	    exit 1; \
	fi

# Load default configuration
.PHONY: defconfig
defconfig: $(KCONFIGLIB)
	@tools/kconfig/defconfig.py --kconfig configs/Kconfig configs/defconfig
	@tools/kconfig/genconfig.py configs/Kconfig

# Interactive configuration
.PHONY: config
config: $(KCONFIGLIB) configs/Kconfig
	@tools/kconfig/menuconfig.py configs/Kconfig
	@tools/kconfig/genconfig.py configs/Kconfig
