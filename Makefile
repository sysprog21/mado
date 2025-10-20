-include .config
include mk/deps.mk

# Set default goal explicitly
.DEFAULT_GOAL := all

check_goal := $(strip $(MAKECMDGOALS))
ifeq ($(filter $(check_goal),config defconfig),)
    ifneq "$(CONFIG_CONFIGURED)" "y"
        $(error You must first run 'make config' or 'make defconfig')
    endif
endif

# Detect Emscripten early (before including toolchain.mk)
CC_VERSION := $(shell $(CC) --version 2>/dev/null)
ifneq ($(findstring emcc,$(CC_VERSION)),)
    CC_IS_EMCC := 1
endif

# Enforce compatible backend for Emscripten builds (skip during config targets)
ifeq ($(filter $(check_goal),config defconfig),)
    ifeq ($(CC_IS_EMCC),1)
        ifneq ($(CONFIG_BACKEND_WASM),y)
            $(error Emscripten (WebAssembly) builds require WASM backend. SDL backend is native-only.)
        endif
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
# Emscripten size optimization
libtwin.a_cflags-$(CC_IS_EMCC) += -Oz

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
libtwin.a_cflags-$(CONFIG_RENDERER_PIXMAN) += $(call dep,cflags,pixman-1)
ifeq ($(CONFIG_RENDERER_PIXMAN), y)
TARGET_LIBS += $(call dep,libs,pixman-1)
endif

# Image loaders

ifeq ($(CONFIG_LOADER_JPEG), y)
libtwin.a_files-y += src/image-jpeg.c
ifneq ($(CC_IS_EMCC), 1)
libtwin.a_cflags-y += $(call dep,cflags,libjpeg)
TARGET_LIBS += $(call dep,libs,libjpeg)
else
# Emscripten libjpeg port - flags needed for both compile and link
libtwin.a_cflags-y += -sUSE_LIBJPEG=1
TARGET_LIBS += -sUSE_LIBJPEG=1
endif
endif

ifeq ($(CONFIG_LOADER_PNG), y)
libtwin.a_files-y += src/image-png.c
ifneq ($(CC_IS_EMCC), 1)
libtwin.a_cflags-y += $(call dep,cflags,libpng)
TARGET_LIBS += $(call dep,libs,libpng)
else
# Emscripten libpng port (includes zlib) - flags needed for both compile and link
libtwin.a_cflags-y += -sUSE_LIBPNG=1 -sUSE_ZLIB=1
TARGET_LIBS += -sUSE_LIBPNG=1 -sUSE_ZLIB=1
endif
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
# Emscripten size optimization
libapps.a_cflags-$(CC_IS_EMCC) += -Oz

# Graphical backends

BACKEND := none

ifeq ($(CONFIG_BACKEND_SDL), y)
BACKEND = sdl
libtwin.a_files-y += backend/sdl.c
libtwin.a_cflags-y += $(call dep,cflags,sdl2)
TARGET_LIBS += $(call dep,libs,sdl2)
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
libtwin.a_cflags-y += $(call dep,cflags,neatvnc aml pixman-1)
TARGET_LIBS += $(call dep,libs,neatvnc aml pixman-1)
endif

ifeq ($(CONFIG_BACKEND_HEADLESS), y)
BACKEND = headless
libtwin.a_files-y += backend/headless.c
endif

ifeq ($(CONFIG_BACKEND_WASM), y)
BACKEND = wasm
libtwin.a_files-y += backend/wasm.c
# WASM backend uses Emscripten directly, no external libraries needed
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
# Emscripten size optimization
demo-$(BACKEND)_cflags-$(CC_IS_EMCC) += -Oz
demo-$(BACKEND)_ldflags-y := \
	$(target.a-y) \
	$(TARGET_LIBS)

# Emscripten-specific linker flags for WebAssembly builds
ifeq ($(CC_IS_EMCC), 1)
# Base Emscripten flags for all backends
demo-$(BACKEND)_ldflags-y += \
	-sINITIAL_MEMORY=33554432 \
	-sALLOW_MEMORY_GROWTH=1 \
	-sSTACK_SIZE=1048576 \
	-sDYNAMIC_EXECUTION=0 \
	-sASSERTIONS=0 \
	-sEXPORTED_FUNCTIONS=_main,_malloc,_free \
	-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU32,HEAP32 \
	-sNO_EXIT_RUNTIME=1 \
	-Oz

# WebAssembly-specific optimizations
ifeq ($(CONFIG_BACKEND_WASM), y)
demo-$(BACKEND)_cflags-y += -flto
demo-$(BACKEND)_ldflags-y += \
	-sMALLOC=emmalloc \
	-sFILESYSTEM=1 \
	--embed-file assets@/assets \
	--exclude-file assets/web \
	-sDISABLE_EXCEPTION_CATCHING=1 \
	-sEXPORT_ES6=0 \
	-sMODULARIZE=0 \
	-sENVIRONMENT=web \
	-sSUPPORT_ERRNO=0 \
	-flto
endif
endif
endif  # CONFIG_DEMO_APPLICATIONS

# Font editor tool
# Tools should not be built for WebAssembly
ifneq ($(CC_IS_EMCC), 1)
ifeq ($(CONFIG_TOOLS), y)
target-$(CONFIG_TOOL_FONTEDIT) += font-edit
font-edit_files-y = \
	tools/font-edit/sfit.c \
	tools/font-edit/font-edit.c
font-edit_includes-y := tools/font-edit
font-edit_cflags-y := \
	$(call dep,cflags,cairo) \
	$(call dep,cflags,sdl2)
font-edit_ldflags-y := \
	$(call dep,libs,cairo) \
	$(call dep,libs,sdl2) \
	-lm

# Headless control tool
target-$(CONFIG_TOOL_HEADLESS_CTL) += headless-ctl
headless-ctl_files-y = tools/headless-ctl.c
headless-ctl_includes-y := include
headless-ctl_ldflags-y := # -lrt
endif
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

# WebAssembly post-build: Copy artifacts to assets/web/
.PHONY: wasm-install
wasm-install:
	@if [ "$(CC_IS_EMCC)" = "1" ]; then \
		echo "Installing WebAssembly artifacts to assets/web/..."; \
		mkdir -p assets/web; \
		cp -f .demo-$(BACKEND)/demo-$(BACKEND) assets/web/demo-$(BACKEND).js 2>/dev/null || true; \
		cp -f .demo-$(BACKEND)/demo-$(BACKEND).wasm assets/web/ 2>/dev/null || true; \
		echo "✓ WebAssembly build artifacts copied to assets/web/"; \
		echo ""; \
		echo "\033[1;32m========================================\033[0m"; \
		echo "\033[1;32mWebAssembly build complete!\033[0m"; \
		echo "\033[1;32m========================================\033[0m"; \
		echo ""; \
		echo "To test in browser, run:"; \
		echo "  \033[1;34m./scripts/serve-wasm.py --open\033[0m"; \
		echo ""; \
	fi

# Override all target to add post-build hook for Emscripten
ifeq ($(CC_IS_EMCC), 1)
all: wasm-install
endif
