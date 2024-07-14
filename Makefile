include mk/common.mk

LIB_SRCS := \
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

LIB_OBJS := $(patsubst %.c, %.o, $(LIB_SRCS))
deps := $(LIB_OBJS:%.o=%.o.d)

LIBSUF ?= .a
LIBTWIN := libtwin$(LIBSUF)

APPS_SRCS = \
	apps/calc.c \
	apps/spline.c \
	apps/clock.c \
	apps/line.c \
	apps/hello.c \
	apps/demo.c

APPS_OBJS = $(patsubst %.c, %.o, $(APPS_SRCS))
deps += $(APPS_OBJS:%.o=%.o.d)

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LDFLAGS := $(shell sdl2-config --libs)

SDL_BACKEND_SRCS = \
	backend/sdl.c \
	apps/twin-sdl.c

SDL_BACKEND_OBJS := $(patsubst %.c, %.o, $(SDL_BACKEND_SRCS))
deps += $(SDL_BACKEND_OBJS:%.o=%.o.d)

.PHONY: all
all: $(LIBTWIN) demo-sdl

demo-sdl: $(LIBTWIN) $(APPS_OBJS) $(SDL_BACKEND_OBJS)
	$(VECHO) "  LD\t$@\n"
	$(Q)$(CC) -o $@ $^ $(SDL_LDFLAGS)

$(LIBTWIN): $(LIB_OBJS)
	$(AR) -r $@ $?

CFLAGS = \
	-Wall -pipe -O2 \
	-I include \
	-I backend \
	-I apps \
	$(SDL_CFLAGS)

%.o: %.c
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) $(CFLAGS) -o $@ -c -MMD -MF $@.d $<

.PHONY: clean
clean:
	@rm -fv demo-sdl \
		$(LIB_OBJS) $(APPS_OBJS) $(SDL_BACKEND_OBJS) $(deps) \
		$(LIBTWIN) | xargs echo --

-include $(deps)
