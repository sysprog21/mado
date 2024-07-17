TARGET = twin-fedit

CFLAGS = $(shell pkg-config --cflags cairo x11) -g -Wall
LIBS = $(shell pkg-config --libs cairo x11)

OBJS = \
	twin-fedit.o \
	sfit.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(TARGET) $(OBJS)
