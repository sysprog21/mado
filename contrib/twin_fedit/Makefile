CFLAGS=$(shell pkg-config --cflags cairo x11) -g -Wall
LIBS=$(shell pkg-config --libs cairo x11)

OBJS=twin-fedit.o twin-sfit.o

twin-fedit: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS): twin-fedit.h
