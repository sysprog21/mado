CFLAGS=$(shell pkg-config --cflags cairo) -g
LIBS=$(shell pkg-config --libs cairo)

OBJS=twin-fedit.o twin-sfit.o

twin-fedit: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS): twin-fedit.h
