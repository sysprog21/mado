TARGET = twin-ttf

CFLAGS = $(shell pkg-config --cflags freetype2) -g -Wall
LIBS = $(shell pkg-config --libs freetype2)

OBJS = \
	twin-ttf.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(TARGET) $(OBJS)
