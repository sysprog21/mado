CFLAGS=-g
INCLUDES= @X_CFLAGS@ @WARN_CFLAGS@

#libtwin_la_SOURCES = \
#	twin.h \
#	twin_draw.c \
#	twin_pixmap.c \
#	twin_primitive.c \
#	twin_screen.c \
#	twin_x11.c \
#	twinint.h
#
#lib_LTLIBRARIES = libtwin.la

bin_PROGRAMS = xtwin

xtwin_SOURCES = \
	twin.h \
	twin_draw.c \
	twin_pixmap.c \
	twin_primitive.c \
	twin_screen.c \
	twin_x11.c \
	twinint.h \
	xtwin.c

xtwin_LDADD = @X_LIBS@
