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
#include <twin_def.h>
#if HAVE_PTHREAD_H
#include <pthread.h>
#endif

typedef uint8_t	    twin_a8_t;
typedef uint16_t    twin_a16_t;
typedef uint16_t    twin_rgb16_t;
typedef uint32_t    twin_argb32_t;
typedef uint32_t    twin_ucs4_t;
typedef int	    twin_bool_t;

#define TWIN_FALSE  0
#define TWIN_TRUE   1

typedef enum { TWIN_A8, TWIN_RGB16, TWIN_ARGB32 } twin_format_t;

#define twin_bytes_per_pixel(format)    (1 << (int) (format))

/*
 * Angles
 */
typedef int16_t	    twin_angle_t;   /* -512 .. 512 for -180 .. 180 */

#define TWIN_ANGLE_360	    1024
#define TWIN_ANGLE_180	    512
#define TWIN_ANGLE_90	    256
#define TWIN_ANGLE_45	    128
#define TWIN_ANGLE_22_5	    64
#define TWIN_ANGLE_11_25    32

#define twin_degrees_to_angle(d)    ((d) * 512 / 180)

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
    int				disable;
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
 * twin_put_begin_t: called before data are drawn to the screen
 * twin_put_span_t: called for each scanline drawn
 */
typedef void	(*twin_put_begin_t) (int x,
				     int y,
				     int width,
				     int height,
				     void *closure);
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
    void		(*damaged) (void *);
    void		*damaged_closure;
    int			disable;
#if HAVE_PTHREAD_H
    pthread_mutex_t	screen_mutex;
#endif
    /*
     * Repaint function
     */
    twin_put_begin_t	put_begin;
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

typedef int32_t	    twin_fixed_t;   /* 16.16 format */

#define TWIN_FIXED_ONE	(0x10000)
#define TWIN_FIXED_HALF	(0x08000)
#define TWIN_FIXED_MAX	(0x7fffffff)
#define TWIN_FIXED_MIN	(-0x7fffffff)

/* 
 * 'double' is a no-no in any shipping code, but useful during
 * development
 */
#define twin_double_to_fixed(d)    ((twin_fixed_t) ((d) * 65536))
#define twin_fixed_to_double(f)    ((double) (f) / 65536.0)

#define twin_int_to_fixed(i)	   ((twin_fixed_t) (i) << 16)

/*
 * Place matrices in structures so they can be easily copied
 */
typedef struct _twin_matrix {
    twin_fixed_t    m[3][2];
} twin_matrix_t;

typedef struct _twin_path twin_path_t;

typedef struct _twin_state {
    twin_matrix_t	matrix;
    twin_fixed_t	font_size;
    int			font_style;
} twin_state_t;

/*
 * Text metrics
 */

typedef struct _twin_text_metrics {
    twin_fixed_t    left_side_bearing;
    twin_fixed_t    right_side_bearing;
    twin_fixed_t    ascent;
    twin_fixed_t    descent;
    twin_fixed_t    width;
    twin_fixed_t    font_ascent;
    twin_fixed_t    font_descent;
} twin_text_metrics_t;

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
 * twin_fixed.c
 */

#if 0
twin_fixed_t
twin_fixed_mul (twin_fixed_t af, twin_fixed_t bf);
#else
#define twin_fixed_mul(a,b)    ((twin_fixed_t) (((int64_t) (a) * (b)) >> 16))
#endif

twin_fixed_t
twin_fixed_sqrt (twin_fixed_t a);

#if 0
twin_fixed_t
twin_fixed_div (twin_fixed_t a, twin_fixed_t b);
#else
#define twin_fixed_div(a,b)	((twin_fixed_t) ((((int64_t) (a)) << 16) / b))
#endif

/*
 * twin_font.c
 */

twin_bool_t
twin_has_ucs4 (twin_ucs4_t ucs4);
    
#define TWIN_TEXT_ROMAN	    0
#define TWIN_TEXT_BOLD	    1
#define TWIN_TEXT_OBLIQUE   2
#define TWIN_TEXT_UNHINTED  4

void
twin_path_ucs4_stroke (twin_path_t *path, twin_ucs4_t ucs4);

void
twin_path_ucs4 (twin_path_t *path, twin_ucs4_t ucs4);
 
void
twin_path_utf8 (twin_path_t *path, const char *string);

twin_fixed_t
twin_width_ucs4 (twin_path_t *path, twin_ucs4_t ucs4);
 
twin_fixed_t
twin_width_utf8 (twin_path_t *path, const char *string);

void
twin_text_metrics_ucs4 (twin_path_t	    *path, 
			twin_ucs4_t	    ucs4, 
			twin_text_metrics_t *m);

void
twin_text_metrics_utf8 (twin_path_t	    *path,
			const char	    *string,
			twin_text_metrics_t *m);
/*
 * twin_hull.c
 */

twin_path_t *
twin_path_convex_hull (twin_path_t *path);

/*
 * twin_matrix.c
 */

void
twin_matrix_identity (twin_matrix_t *m);

void
twin_matrix_translate (twin_matrix_t *m, twin_fixed_t tx, twin_fixed_t ty);

void
twin_matrix_scale (twin_matrix_t *m, twin_fixed_t sx, twin_fixed_t sy);

void
twin_matrix_rotate (twin_matrix_t *m, twin_angle_t a);

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
twin_path_empty (twin_path_t *path);

void
twin_path_bounds (twin_path_t *path, twin_rect_t *rect);

void
twin_path_append (twin_path_t *dst, twin_path_t *src);

twin_path_t *
twin_path_create (void);

void
twin_path_destroy (twin_path_t *path);

void
twin_path_identity (twin_path_t *path);

void
twin_path_translate (twin_path_t *path, twin_fixed_t tx, twin_fixed_t ty);

void
twin_path_scale (twin_path_t *path, twin_fixed_t sx, twin_fixed_t sy);

void
twin_path_rotate (twin_path_t *path, twin_angle_t a);

twin_matrix_t
twin_path_current_matrix (twin_path_t *path);

void
twin_path_set_matrix (twin_path_t *path, twin_matrix_t matrix);

twin_fixed_t
twin_path_current_font_size (twin_path_t *path);

void
twin_path_set_font_size (twin_path_t *path, twin_fixed_t font_size);

int
twin_path_current_font_style (twin_path_t *path);

void
twin_path_set_font_style (twin_path_t *path, int font_style);

twin_state_t
twin_path_save (twin_path_t *path);

void
twin_path_restore (twin_path_t *path, twin_state_t *state);

void
twin_composite_path (twin_pixmap_t	*dst,
		     twin_operand_t	*src,
		     int		src_x,
		     int		src_y,
		     twin_path_t	*path,
		     twin_operator_t	operator);
void
twin_paint_path (twin_pixmap_t	*dst,
		 twin_argb32_t	argb,
		 twin_path_t	*path);

void
twin_composite_stroke (twin_pixmap_t	*dst,
		       twin_operand_t	*src,
		       int		src_x,
		       int		src_y,
		       twin_path_t	*stroke,
		       twin_fixed_t	pen_width,
		       twin_operator_t	operator);

void
twin_paint_stroke (twin_pixmap_t    *dst,
		   twin_argb32_t    argb,
		   twin_path_t	    *stroke,
		   twin_fixed_t	    pen_width);

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
twin_pixmap_enable_update (twin_pixmap_t *pixmap);

void
twin_pixmap_disable_update (twin_pixmap_t *pixmap);

void
twin_pixmap_damage (twin_pixmap_t *pixmap,
		    int x1, int y1, int x2, int y2);

void
twin_pixmap_lock (twin_pixmap_t *pixmap);

void
twin_pixmap_unlock (twin_pixmap_t *pixmap);

void
twin_pixmap_move (twin_pixmap_t *pixmap, int x, int y);

twin_pointer_t
twin_pixmap_pointer (twin_pixmap_t *pixmap, int x, int y);

/*
 * twin_poly.c
 */
void
twin_fill_path (twin_pixmap_t *pixmap, twin_path_t *path, int dx, int dy);

/*
 * twin_screen.c
 */

twin_screen_t *
twin_screen_create (int			width,
		    int			height, 
		    twin_put_begin_t	put_begin,
		    twin_put_span_t	put_span,
		    void		*closure);

void
twin_screen_destroy (twin_screen_t *screen);

void
twin_screen_enable_update (twin_screen_t *screen);

void
twin_screen_disable_update (twin_screen_t *screen);

void
twin_screen_damage (twin_screen_t *screen,
		    int x1, int y1, int x2, int y2);
		    
void
twin_screen_register_damaged (twin_screen_t *screen, 
			      void (*damaged) (void *),
			      void *closure);

void
twin_screen_resize (twin_screen_t *screen, int width, int height);

twin_bool_t
twin_screen_damaged (twin_screen_t *screen);

void
twin_screen_update (twin_screen_t *screen);

void
twin_screen_lock (twin_screen_t *screen);

void
twin_screen_unlock (twin_screen_t *screen);


/*
 * twin_spline.c
 */

void
twin_path_curve (twin_path_t	*path,
		 twin_fixed_t	x1, twin_fixed_t y1,
		 twin_fixed_t	x2, twin_fixed_t y2,
		 twin_fixed_t	x3, twin_fixed_t y3);

/*
 * twin_trig.c
 */

twin_fixed_t
twin_sin (twin_angle_t a);

twin_fixed_t
twin_cos (twin_angle_t a);

twin_fixed_t
twin_tan (twin_angle_t a);

#endif /* _TWIN_H_ */
