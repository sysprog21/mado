CFLAGS=-g
INCLUDES= @X_CFLAGS@ @WARN_CFLAGS@
SUBDIRS=twin_ttf

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
	twin_convolve.c \
	twin_draw.c \
	twin_glyphs.c \
	twin_hull.c \
	twin_fixed.c \
	twin_font.c \
	twin_geom.c \
	twin_matrix.c \
	twin_path.c \
	twin_pixmap.c \
	twin_poly.c \
	twin_primitive.c \
	twin_screen.c \
	twin_spline.c \
	twin_trig.c \
	twin_x11.c \
	twinint.h \
	xtwin.c

xtwin_LDADD = @X_LIBS@ -lm
