/*
 * $Id$
 *
 * Copyright © 2004 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _TWIN_H_
#define _TWIN_H_

#include <stdlib.h>
#include <stdint.h>

typedef uint8_t	    twin_a8_t;
typedef uint16_t    twin_a16_t;
typedef uint16_t    twin_rgb16_t;
typedef uint32_t    twin_argb32_t;
typedef int	    twin_bool_t;
typedef int16_t	    twin_fixed_t;   /* 12.4 format */
typedef int32_t	    twin_dfixed_t;

#define twin_fixed_floor(f) ((((f) < 0) ? ((f) + 0xf) : (f)) >> 4)
#define twin_fixed_trunc(f) ((f) >> 4)

#define twin_double_to_fixed(d)	((twin_fixed_t) ((d) * 16.0))
#define twin_fixed_to_double(f)	((double) (f) / 16.0)

#define TWIN_FIXED_ONE		(0x10)
#define TWIN_FIXED_TOLERANCE	(TWIN_FIXED_ONE >> 1)

#define TWIN_FALSE  0
#define TWIN_TRUE   1

typedef enum { TWIN_A8, TWIN_RGB16, TWIN_ARGB32 } twin_format_t;

#define twin_bytes_per_pixel(format)    (1 << (int) (format))

/*
 * A rectangle
 */
typedef struct _twin_rect {
    int	    left, right, top, bottom;
} twin_rect_t;

typedef union _twin_pointer {
    void	    *v;
    uint8_t	    *b;
    twin_a8_t	    *a8;
    twin_rgb16_t    *rgb16;
    twin_argb32_t   *argb32;
} twin_pointer_t;

/*
 * A rectangular array of pixels
 */
typedef struct _twin_pixmap {
    /*
     * Screen showing these pixels
     */
    struct _twin_screen		*screen;
    /*
     * List of displayed pixmaps
     */
    struct _twin_pixmap		*higher;
    /*
     * Screen position
     */
    int			x, y;
    /*
     * Pixmap layout
     */
    twin_format_t	format;
    int			width;	    /* pixels */
    int			height;	    /* pixels */
    int			stride;	    /* bytes */
    /*
     * Pixels
     */
    twin_pointer_t	p;
} twin_pixmap_t;

/*
 * A function that paints pixels to the screen
 */
typedef void	(*twin_put_span_t) (int x,
				    int y,
				    int width,
				    twin_argb32_t *pixels,
				    void *closure);

/*
 * A screen
 */
typedef struct _twin_screen {
    /*
     * List of displayed pixmaps
     */
    twin_pixmap_t	*bottom;
    /*
     * Output size
     */
    int			width, height;
    /*
     * Damage
     */
    twin_rect_t		damage;
    /*
     * Repaint function
     */
    twin_put_span_t	put_span;
    void		*closure;
} twin_screen_t;

/*
 * A source operand
 */

typedef enum { TWIN_SOLID, TWIN_PIXMAP } twin_source_t;

typedef struct _twin_operand {
    twin_source_t	source_kind;
    union {
	twin_pixmap_t	*pixmap;
	twin_argb32_t	argb;
    }			u;
} twin_operand_t;

typedef enum { TWIN_OVER, TWIN_SOURCE } twin_operator_t;

/*
 * A (fixed point) point
 */

typedef struct _twin_point {
    twin_fixed_t    x, y;
} twin_point_t;

typedef struct _twin_path twin_path_t;

/*
 * twin_convolve.c
 */
void
twin_path_convolve (twin_path_t	*dest,
		    twin_path_t	*stroke,
		    twin_path_t	*pen);

/*
 * twin_draw.c
 */

void
twin_composite (twin_pixmap_t	*dst,
		int		dst_x,
		int		dst_y,
		twin_operand_t	*src,
		int		src_x,
		int		src_y,
		twin_operand_t	*msk,
		int		msk_x,
		int		msk_y,
		twin_operator_t	operator,
		int		width,
		int		height);

void
twin_fill (twin_pixmap_t    *dst,
	   twin_argb32_t    pixel,
	   twin_operator_t  operator,
	   int		    x,
	   int		    y,
	   int		    width,
	   int		    height);

/*
 * twin_path.c
 */

void 
twin_path_move (twin_path_t *path, twin_fixed_t x, twin_fixed_t y);

void
twin_path_draw (twin_path_t *path, twin_fixed_t x, twin_fixed_t y);

void
twin_path_circle(twin_path_t *path, twin_fixed_t radius);

void
twin_path_close (twin_path_t *path);

void
twin_path_fill (twin_pixmap_t *pixmap, twin_path_t *path);

void
twin_path_empty (twin_path_t *path);

twin_path_t *
twin_path_create (void);

void
twin_path_destroy (twin_path_t *path);

/*
 * twin_pixmap.c
 */

twin_pixmap_t *
twin_pixmap_create (twin_format_t format, int width, int height);

void
twin_pixmap_destroy (twin_pixmap_t *pixmap);

void
twin_pixmap_show (twin_pixmap_t *pixmap, 
		  twin_screen_t	*screen,
		  twin_pixmap_t *higher);

void
twin_pixmap_hide (twin_pixmap_t *pixmap);

void
twin_pixmap_damage (twin_pixmap_t *pixmap,
		    int x1, int y1, int x2, int y2);

void
twin_pixmap_move (twin_pixmap_t *pixmap, int x, int y);

twin_pointer_t
twin_pixmap_pointer (twin_pixmap_t *pixmap, int x, int y);

/*
 * twin_poly.c
 */
void
twin_polygon (twin_pixmap_t *pixmap, twin_point_t *vertices, int nvertices);

/*
 * twin_screen.c
 */

twin_screen_t *
twin_screen_create (int		    width,
		    int		    height, 
		    twin_put_span_t put_span,
		    void	    *closure);

void
twin_screen_destroy (twin_screen_t *screen);

void
twin_screen_damage (twin_screen_t *screen,
		    int x1, int y1, int x2, int y2);
		    
void
twin_screen_resize (twin_screen_t *screen, int width, int height);

twin_bool_t
twin_screen_damaged (twin_screen_t *screen);

void
twin_screen_update (twin_screen_t *screen);

/*
 * twin_spline.c
 */

void
twin_path_curve (twin_path_t	*path,
		 twin_fixed_t	x1, twin_fixed_t y1,
		 twin_fixed_t	x2, twin_fixed_t y2,
		 twin_fixed_t	x3, twin_fixed_t y3);

/*
 * twin_x11.c 
 */

#include <X11/Xlib.h>

typedef struct _twin_x11 {
    twin_screen_t   *screen;
    Display	    *dpy;
    Window	    win;
    GC		    gc;
    Visual	    *visual;
    int		    depth;
} twin_x11_t;

twin_x11_t *
twin_x11_create (Display *dpy, int width, int height);

void
twin_x11_destroy (twin_x11_t *tx);

void
twin_x11_damage (twin_x11_t *tx, XExposeEvent *ev);

void
twin_x11_configure (twin_x11_t *tx, XConfigureEvent *ev);

void
twin_x11_update (twin_x11_t *tx);

#endif /* _TWIN_H_ */
