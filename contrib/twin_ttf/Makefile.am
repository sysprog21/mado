CFLAGS=-g
INCLUDES= @WARN_CFLAGS@ @FREETYPE_CFLAGS@

bin_PROGRAMS = twin_ttf

twin_ttf_SOURCES = \
	twin_ttf.h \
	twin_ttf.c

twin_ttf_LDADD = @FREETYPE_LIBS@ -lm
