-include .config

check_goal := $(strip $(MAKECMDGOALS))
ifneq ($(check_goal), config)
ifneq "$(CONFIG_CONFIGURED)" "y"
$(error You must first run 'make config')
endif
endif

# Rules

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
	src/primitive.c \
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
	src/hull.c \
	src/icon.c \
	src/pixmap.c \
	src/timeout.c \
	src/image.c \
	src/animation.c \
	src/api.c

libtwin.a_includes-y := \
	include \
	src

# Features
libtwin.a_files-$(CONFIG_LOGGING) += src/log.c
libtwin.a_files-$(CONFIG_CURSOR) += src/cursor.c

# Renderer
libtwin.a_files-$(CONFIG_RENDERER_BUILTIN) += src/draw.c
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
TARGET_LIBS += -ludev
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

# Standalone application

ifeq ($(CONFIG_DEMO_APPLICATIONS), y)
target-y += demo-$(BACKEND)
demo-$(BACKEND)_depends-y += $(target.a-y)
demo-$(BACKEND)_files-y = apps/main.c
demo-$(BACKEND)_includes-y := include
demo-$(BACKEND)_ldflags-y := \
	$(target.a-y) \
	$(TARGET_LIBS)
endif

CFLAGS += -include config.h

check_goal := $(strip $(MAKECMDGOALS))
ifneq ($(check_goal), config)
include mk/common.mk
endif

# Menuconfig
.PHONY: config
config: configs/Kconfig
	@tools/kconfig/menuconfig.py $<
	@tools/kconfig/genconfig.py $<
