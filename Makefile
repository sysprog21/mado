# FIXME: make these entries configurable
CONFIG_BACKEND_SDL := y
CONFIG_LOADER_PNG := y

# Rules

target-y :=
target.o-y :=
TARGET_LIBS :=

target.a-y = \
	libtwin.a \
	libbackend.a \
	libapps.a

# Core library

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
	src/timeout.c

libtwin.a_includes-y := include

# Image loaders

ifeq ($(CONFIG_LOADER_PNG), y)
target.o-y += src/image-png.o
src/image-png.o_files-y = src/image-png.c
src/image-png.o_includes-y := include
src/image-png.o_cflags-y = $(shell pkg-config --cflags libpng)
TARGET_LIBS += $(shell pkg-config --libs libpng)
libtwin.a_files-y += src/image-png.o
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

libbackend.a_files-y :=
libbackend.a_cflags-y :=
libbackend.a_includes-y := include

ifeq ($(CONFIG_BACKEND_SDL), y)
libbackend.a_files-y += backend/sdl.c
libbackend.a_cflags-y += $(shell sdl2-config --cflags)
TARGET_LIBS += $(shell sdl2-config --libs)
endif

# Platform-specific executables

ifeq ($(CONFIG_BACKEND_SDL), y)
target-y += demo-sdl

demo-sdl_depends-y += $(target.a-y)
demo-sdl_files-y = \
	apps/twin-sdl.c

demo-sdl_includes-y := include
demo-sdl_includes-y += backend

demo-sdl_cflags-y = $(shell sdl2-config --cflags)
demo-sdl_ldflags-y := $(target.a-y)
demo-sdl_ldflags-y += $(TARGET_LIBS)
endif # CONFIG_BACKEND_SDL

include mk/common.mk
