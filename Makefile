# FIXME: make these entries configurable
CONFIG_BACKEND_SDL ?= y
CONFIG_LOADER_JPEG ?= y
CONFIG_LOADER_PNG ?= y

# Rules

target-y :=
target.o-y :=
TARGET_LIBS :=

target.a-y = \
	libapps.a \
	libtwin.a

# Core library

libtwin.a_cflags-y :=

libtwin.a_files-y = \
	src/box.c \
	src/file.c \
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
	src/draw.c \
	src/hull.c \
	src/icon.c \
	src/pixmap.c \
	src/timeout.c \
	src/api.c

libtwin.a_includes-y := \
	include \
	src

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

# Applications

libapps.a_files-y = \
	apps/calc.c \
	apps/spline.c \
	apps/clock.c \
	apps/line.c \
	apps/hello.c \
	apps/demo.c

libapps.a_includes-y := include

# Graphical backends
BACKEND := none

ifeq ($(CONFIG_BACKEND_SDL), y)
BACKEND = sdl
libtwin.a_files-y += backend/sdl.c
libtwin.a_cflags-y += $(shell sdl2-config --cflags)
TARGET_LIBS += $(shell sdl2-config --libs)
endif

# Platform-specific executables
target-y += demo-$(BACKEND)
demo-$(BACKEND)_depends-y += $(target.a-y)
demo-$(BACKEND)_files-y = apps/main.c
demo-$(BACKEND)_includes-y := include
demo-$(BACKEND)_ldflags-y := \
	$(target.a-y) \
	$(TARGET_LIBS)

include mk/common.mk
