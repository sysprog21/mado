dnl 
dnl  $Id$
dnl 
dnl  Copyright © 2003 Keith Packard, Noah Levitt
dnl 
dnl  Permission to use, copy, modify, distribute, and sell this software and its
dnl  documentation for any purpose is hereby granted without fee, provided that
dnl  the above copyright notice appear in all copies and that both that
dnl  copyright notice and this permission notice appear in supporting
dnl  documentation, and that the name of Keith Packard not be used in
dnl  advertising or publicity pertaining to distribution of the software without
dnl  specific, written prior permission.  Keith Packard makes no
dnl  representations about the suitability of this software for any purpose.  It
dnl  is provided "as is" without express or implied warranty.
dnl 
dnl  KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
dnl  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
dnl  EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
dnl  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
dnl  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
dnl  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
dnl  PERFORMANCE OF THIS SOFTWARE.
dnl
dnl Process this file with autoconf to create configure.

AC_PREREQ([2.57])

AC_INIT(libtwin, 0.0.0, [keithp@keithp.com], libtwin)
AM_INIT_AUTOMAKE()
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(twin_def.h)
AC_CONFIG_AUX_DIR(.)

# Check for progs
AC_PROG_CC
AC_PROG_LIBTOOL

WARN_CFLAGS=""

if test "x$GCC" = "xyes"; then
	WARN_CFLAGS="-Wall -Wpointer-arith -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations \
	-Wnested-externs -fno-strict-aliasing"
fi
AC_SUBST(WARN_CFLAGS)

# Check for X
PKG_CHECK_MODULES(X, x11, 
		  [x_found_with_pkgconfig=yes],
		  [x_found_with_pkgconfig=no])
AC_SUBST(X_CFLAGS)
AC_SUBST(X_LIBS)

AC_CHECK_LIB([pthread], [pthread_create])

AC_CHECK_HEADERS([pthread.h])

#
#AC_ARG_WITH(freetype-config, [  --with-freetype-config=PROG   Use FreeType configuration program PROG], freetype_config=$withval, freetype_config=yes)
#
#if test "$freetype_config" = "yes"; then 
#	AC_PATH_PROG(ft_config,freetype-config,no)
#	if test "$ft_config" = "no"; then
#		AC_MSG_ERROR([You must have freetype installed; see http://www.freetype.org/])
#	fi
#else
#	ft_config="$freetype_config"
#fi
#
#FREETYPE_CFLAGS="`$ft_config --cflags`"
#FREETYPE_LIBS="`$ft_config --libs`"
#
#AC_SUBST(FREETYPE_LIBS)
#AC_SUBST(FREETYPE_CFLAGS)

AC_OUTPUT([Makefile
           twin.pc])
