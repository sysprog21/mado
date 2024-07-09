LIBSUF ?= .a

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
	src/pixmap.c \
	src/timeout.c 

LIB_OBJS := $(patsubst %.c, %.o, $(LIB_SRCS))
PRODUCTS := libtwin$(LIBSUF)

DEMO_SRCS = \
	apps/calc.c \
	apps/spline.c \
	apps/clock.c \
	apps/line.c \
	apps/hello.c \
	apps/text.c \
	apps/demo.c

DEMO_OBJS = $(patsubst %.c, %.o, $(DEMO_SRCS))

SDL_BACKEND_SRCS = \
	backend/sdl.c \
	apps/twin-sdl.c

SDL_BACKEND_OBJS := $(patsubst %.c, %.o, $(SDL_BACKEND_SRCS))

.PHONY: all
all: $(PRODUCTS) demo-sdl

demo-sdl: $(PRODUCTS) $(DEMO_OBJS) $(SDL_BACKEND_OBJS)
	$(CC) $(CFLAGS) -o $@ \
		$(DEMO_OBJS) $(SDL_BACKEND_SRCS) $(PRODUCTS) \
		$(shell sdl2-config --libs)

%$(LIBSUF): $(LIB_OBJS)
	$(AR) -r $@ $?

CFLAGS = \
	-Wall -pipe -Os -ffast-math \
	-I include \
	-I backend \
	-I apps \
    $(shell sdl2-config --cflags)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $< -lm

.PHONY: clean
clean:
	@rm -fv demo-sdl \
		$(LIB_OBJS) $(DEMO_OBJS) \
		$(SDL_BACKEND_OBJS) \
		*$(LIBSUF) | xargs echo --
