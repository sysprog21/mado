include mk/common.mk

CFLAGS = \
	-Wall -pipe -O2 \
	-I include \
	-I backend \
	-I apps \
	$(shell pkg-config --cflags libpng)

LDFLAGS = \
	$(shell pkg-config --libs libpng)

deps :=

LIB_OBJS := \
	src/box.o \
	src/file.o \
	src/poly.o \
	src/toplevel.o \
	src/button.o \
	src/fixed.o \
	src/label.o \
	src/primitive.o \
	src/trig.o \
	src/convolve.o \
	src/font.o \
	src/matrix.o \
	src/queue.o \
	src/widget.o \
	src/font_default.o \
	src/path.o \
	src/screen.o \
	src/window.o \
	src/dispatch.o \
	src/geom.o \
	src/pattern.o \
	src/spline.o \
	src/work.o \
	src/draw.o \
	src/hull.o \
	src/icon.o \
	src/image-png.o \
	src/pixmap.o \
	src/timeout.o

deps += $(LIB_OBJS:%.o=%.o.d)

LIBSUF ?= .a
LIBTWIN := libtwin$(LIBSUF)

APPS_OBJS := \
	apps/calc.o \
	apps/spline.o \
	apps/clock.o \
	apps/line.o \
	apps/hello.o \
	apps/demo.o

deps += $(APPS_OBJS:%.o=%.o.d)

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LDFLAGS := $(shell sdl2-config --libs)

CFLAGS += $(SDL_CFLAGS)

SDL_BACKEND_OBJS := \
	backend/sdl.o \
	apps/twin-sdl.o

deps += $(SDL_BACKEND_OBJS:%.o=%.o.d)

.PHONY: all
all: $(LIBTWIN) demo-sdl

demo-sdl: $(APPS_OBJS) $(SDL_BACKEND_OBJS) $(LIBTWIN)
	$(VECHO) "  LD\t$@\n"
	$(Q)$(CC) -o $@ $^ $(SDL_LDFLAGS) $(LDFLAGS)

$(LIBTWIN): $(LIB_OBJS)
	$(AR) -r $@ $?

%.o: %.c
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) $(CFLAGS) -o $@ -c -MMD -MF $@.d $<

.PHONY: clean
clean:
	@rm -fv demo-sdl \
		$(LIB_OBJS) $(APPS_OBJS) $(SDL_BACKEND_OBJS) $(deps) \
		$(LIBTWIN) | xargs echo --

-include $(deps)
