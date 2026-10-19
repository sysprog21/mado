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

# All translation units must share the arithmetic policy and target ABI.
ifeq ($(CONFIG_MATH_FLOAT),y)
TARGET_LIBS += -lm
endif
ISA_CFLAGS := $(subst ",,$(CONFIG_ISA_CFLAGS))
override CFLAGS += $(ISA_CFLAGS)

src/math-trig.h: scripts/gen-math.py
	@echo "  GEN        $@"
	@python3 $< --trig-table -o $@

src/math-gen.h: scripts/gen-math.py
	@echo "  GEN        $@"
	@python3 $< -o $@

# Core library

libtwin.a_cflags-y :=
# Emscripten size optimization
libtwin.a_cflags-$(CC_IS_EMCC) += -Oz

libtwin.a_files-y = \
	src/poly.c \
	src/fixed.c \
	src/trig.c \
	src/convolve.c \
	src/font.c \
	src/text.c \
	src/cache.c \
	src/matrix.c \
	src/queue.c \
	src/font_default.c \
	src/path.c \
	src/screen.c \
	src/window.c \
	src/wm.c \
	src/geom.c \
	src/pattern.c \
	src/spline.c \
	src/work.c \
	src/hull.c \
	src/pixmap.c \
	src/timeout.c \
	src/anim.c \
	src/image.c \
	src/animation.c \
	src/closure.c \
	src/api.c

# Window manager policy: exactly one is built
libtwin.a_files-$(CONFIG_WM_NONE) += src/wm-none.c
libtwin.a_files-$(CONFIG_WM_DESKTOP) += src/wm-desktop.c src/icon.c
libtwin.a_files-$(CONFIG_WM_MOBILE) += src/wm-mobile.c

# Box layout toolkit
libtwin.a_files-$(CONFIG_WIDGETS) += \
	src/widget.c \
	src/box.c \
	src/label.c \
	src/button.c \
	src/toplevel.c

# Fonts
libtwin.a_files-$(CONFIG_FONT_MONTSERRAT) += src/font-montserrat.c
libtwin.a_files-$(CONFIG_FONT_ICONS) += src/font-icons.c

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
libtwin.a_files-y += src/memstats.c
libtwin.a_files-$(CONFIG_MEM_TLSF) += src/mem-tlsf.c

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
libapps.a_files-$(CONFIG_DEMO_APPLICATIONS) += apps/apps.c
libapps.a_files-$(CONFIG_DEMO_MULTI) += apps/multi.c
libapps.a_files-$(CONFIG_DEMO_CLOCK) += apps/clock.c
libapps.a_files-$(CONFIG_DEMO_CALCULATOR) += apps/calc.c
libapps.a_files-$(CONFIG_DEMO_SPLINE) += apps/spline.c
libapps.a_files-$(CONFIG_DEMO_ANIMATION) += apps/animation.c
libapps.a_files-$(CONFIG_DEMO_IMAGE) += apps/image.c

libapps.a_files-$(CONFIG_DEMO_CELLPHONE) += \
	apps/cellphone/ui.c \
	apps/cellphone/ui-paint.c \
	apps/cellphone/ui-input.c \
	apps/cellphone/shell.c \
	apps/cellphone/home.c \
	apps/cellphone/lock.c \
	apps/cellphone/data.c \
	apps/cellphone/calc.c \
	apps/cellphone/dialer.c \
	apps/cellphone/contacts.c \
	apps/cellphone/sms.c \
	apps/cellphone/music.c \
	apps/cellphone/photos.c \
	apps/cellphone/camera.c \
	apps/cellphone/settings.c \
	apps/cellphone/calllog.c \
	apps/cellphone/games.c \
	apps/cellphone/snake.c \
	apps/cellphone/pong.c \
	apps/cellphone/tetris.c

libapps.a_includes-y := include apps apps/cellphone
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

ifeq ($(CONFIG_BACKEND_DRM), y)
BACKEND = drm
TARGET_LIBS += -ludev -pthread
libtwin.a_files-y += backend/drm.c
libtwin.a_files-y += backend/linux_input.c
libtwin.a_cflags-y += $(shell pkg-config --cflags libdrm)
TARGET_LIBS += $(shell pkg-config --libs libdrm)
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
mado-perf_includes-y := include src
mado-perf_ldflags-y := \
	$(target.a-y) \
	$(TARGET_LIBS)
endif

# Standalone application

ifeq ($(CONFIG_DEMO_APPLICATIONS), y)
target-y += demo-$(BACKEND)
demo-$(BACKEND)_depends-y += $(target.a-y)
demo-$(BACKEND)_files-y = apps/main.c
demo-$(BACKEND)_includes-y := include apps/cellphone
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
# Shares the shared-memory layout only; it does not link libtwin.a.
headless-ctl_cflags-y := -DTWIN_MATH_NO_ABI_CHECK
headless-ctl_ldflags-y := # -lrt
endif
endif

# Headless tests: built only for 'make check' / 'make check-update'
# (and listed for 'make clean' so their binaries are removed)
TESTS := test-core test-wm test-lifetime
ifeq ($(CONFIG_DEMO_APPLICATIONS), y)
TESTS += test-demos
endif
ifeq ($(CONFIG_DEMO_CELLPHONE), y)
TESTS += test-cellphone
endif

ifneq ($(filter check check-update clean distclean,$(MAKECMDGOALS)),)
ifneq ($(CC_IS_EMCC), 1)
define test-target
target-y += $(1)
$(1)_depends-y += $(target.a-y)
$(1)_files-y := tests/$(1).c tests/harness.c \
	$(if $(filter test-cellphone,$(1)),$(wildcard tests/phone-*.c))
$(1)_includes-y := include src tests apps apps/cellphone
$(1)_ldflags-y := $(target.a-y) $(TARGET_LIBS)
endef
$(foreach t,$(TESTS),$(eval $(call test-target,$(t))))
endif
endif

# Arithmetic tests use their own main, without the rendering harness.
ifneq ($(filter check check-update clean distclean,$(MAKECMDGOALS)),)
ifneq ($(CC_IS_EMCC),1)
TESTS += test-math
target-y += test-math
test-math_depends-y := libtwin.a
test-math_files-y := tests/test-math.c
test-math_includes-y := include src
test-math_ldflags-y := libtwin.a $(TARGET_LIBS) -lm
endif
endif

.PHONY: check-math
check-math:
	@python3 scripts/check-math.py --cc "$(HOSTCC)" --sanitize

# Goals that build targets, so header dependencies apply to them too
extra-build-goals := check check-update

.PHONY: check check-update
ifeq ($(CC_IS_EMCC), 1)
check check-update:
	@echo "The headless tests run natively; rebuild without Emscripten" >&2
	@exit 1
else
check: $(TESTS)
	@mkdir -p tests/golden
	@status=0; for t in $(TESTS); do ./$$t || status=1; done; exit $$status

check-update: $(TESTS)
	@mkdir -p tests/golden
	@status=0; for t in $(TESTS); do MADO_UPDATE_GOLDEN=1 ./$$t || status=1; done; exit $$status
endif

# Build system integration

override CFLAGS += -include config.h
ifeq ($(CONFIG_OPTIMIZE_SIZE), y)
# After -O2 in the common flags, so it wins
override CFLAGS += -Os -fno-asynchronous-unwind-tables -fno-unwind-tables
endif

# Ensure composite-decls.h exists before including build rules
# (needed for dependency generation in mk/common.mk)
ifeq ($(filter config defconfig clean distclean,$(MAKECMDGOALS)),)
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

# Kconfig offers RISC-V profiles only for a RISC-V compiler; catch CC changes
# made after configuration.
ifneq ($(ISA_CFLAGS),)
ifeq ($(filter config defconfig clean distclean,$(check_goal)),)
    ifeq ($(findstring riscv,$(TARGET_MACHINE)),)
        $(error RISC-V ISA profile selected, but '$(CC)' targets \
            '$(subst $(empty) $(empty),-,$(TARGET_MACHINE))'. Set CROSS_COMPILE \
            (or CC) to a RISC-V compiler, or select "Compiler default" in \
            'make config')
    endif
endif
endif

KCONFIGLIB_REPO := https://github.com/sysprog21/Kconfiglib
KCONFIGLIB := tools/kconfig/kconfiglib.py
$(KCONFIGLIB):
	$(RM) -r tools/kconfig
	git clone --depth=1 $(KCONFIGLIB_REPO) tools/kconfig

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

# Remove Kconfiglib and build artifacts
.PHONY: distclean
distclean: clean
	$(RM) -r tools/kconfig

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
