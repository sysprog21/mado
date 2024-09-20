TARGET = twin-fedit

CFLAGS = $(shell pkg-config --cflags cairo) $(shell sdl2-config --cflags) -g -Wall 
LIBS = $(shell pkg-config --libs cairo) $(shell sdl2-config --libs)

OBJS = \
	twin-fedit.o \
	sfit.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(TARGET) $(OBJS)
