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

#ifndef _TWININT_H_
#define _TWININT_H_

#include "twin.h"
#include <string.h>

/*
 * Post-transformed points are stored in 12.4 fixed point
 * values
 */

typedef int16_t	    twin_sfixed_t;  /* 12.4 format */
typedef int32_t	    twin_dfixed_t;  /* 24.8 format (12.4 * 12.4) */

#define twin_sfixed_floor(f)	    ((f) & ~0xf)
#define twin_sfixed_trunc(f)	    ((f) >> 4)
#define twin_sfixed_ceil(f)	    (((f) + 0xf) & ~0xf)

#define twin_int_to_sfixed(i)	    ((twin_sfixed_t) ((i) * 16))

#define twin_sfixed_to_fixed(s)	    (((twin_fixed_t) (s)) << 12)
#define twin_fixed_to_sfixed(f)	    ((twin_sfixed_t) ((f) >> 12))

#define twin_sfixed_to_dfixed(s)    (((twin_dfixed_t) (s)) << 4)

/* 
 * 'double' is a no-no in any shipping code, but useful during
 * development
 */
#define twin_double_to_sfixed(d)    ((twin_sfixed_t) ((d) * 16.0))
#define twin_sfixed_to_double(f)    ((double) (f) / 16.0)

#define TWIN_SFIXED_ONE		(0x10)
#define TWIN_SFIXED_HALF	(0x08)
#define TWIN_SFIXED_TOLERANCE	(TWIN_SFIXED_ONE >> 2)
#define TWIN_SFIXED_MIN		(-0x7fff)
#define TWIN_SFIXED_MAX		(0x7fff)

/*
 * Glyph coordinates are stored in 2.6 fixed point
 */

typedef signed char	twin_gfixed_t;

/*
 * Compositing stuff
 */
#define twin_int_mult(a,b,t)	((t) = (a) * (b) + 0x80, \
				 ((((t)>>8 ) + (t))>>8 ))
#define twin_int_div(a,b)	(((uint16_t) (a) * 255) / (b))
#define twin_get_8(v,i)		((uint16_t) (uint8_t) ((v) >> (i)))
#define twin_sat(t)		((uint8_t) ((t) | (0 - ((t) >> 8))))

#define twin_in(s,i,m,t)	\
    ((twin_argb32_t) twin_int_mult (twin_get_8(s,i),(m),(t)) << (i))

#define twin_over(s,d,i,m,t)	\
    (((t) = twin_int_mult(twin_get_8(d,i),(m),(t)) + twin_get_8(s,i)),\
     (twin_argb32_t) twin_sat (t) << (i))

#define twin_argb32_to_rgb16(s)    ((((s) >> 3) & 0x001f) | \
				    (((s) >> 5) & 0x07e0) | \
				    (((s) >> 8) & 0xf800))
#define twin_rgb16_to_argb32(s) \
    (((((s) << 3) & 0xf8) | (((s) >> 2) & 0x7)) | \
     ((((s) << 5) & 0xfc00) | (((s) >> 1) & 0x300)) | \
     ((((s) << 8) & 0xf80000) | (((s) << 3) & 0x70000)) | \
     0xff000000)

typedef union {
    twin_pointer_t  p;
    twin_argb32_t   c;
} twin_source_u;

typedef void (*twin_src_msk_op) (twin_pointer_t dst,
				 twin_source_u	src,
				 twin_source_u	msk,
				 int		width);

typedef void (*twin_src_op) (twin_pointer_t dst,
			     twin_source_u  src,
			     int	    width);

/* twin_primitive.c */

typedef void twin_in_op_func (twin_pointer_t	dst,
			      twin_source_u	src,
			      twin_source_u	msk,
			      int		width);

typedef void twin_op_func (twin_pointer_t	dst,
			   twin_source_u	src,
			   int			width);

/* Geometrical objects */

typedef struct _twin_spoint {
    twin_sfixed_t    x, y;
} twin_spoint_t;

struct _twin_path {
    twin_spoint_t   *points;
    int		    size_points;
    int		    npoints;
    int		    *sublen;
    int		    size_sublen;
    int		    nsublen;
    twin_state_t    state;
};

typedef struct _twin_gpoint { twin_gfixed_t x, y; } twin_gpoint_t;

/*
 * This needs to be refactored to reduce the number of functions...
 */
twin_in_op_func _twin_argb32_in_argb32_over_argb32;
twin_in_op_func _twin_argb32_in_rgb16_over_argb32;
twin_in_op_func _twin_argb32_in_a8_over_argb32;
twin_in_op_func _twin_argb32_in_c_over_argb32;
twin_in_op_func _twin_rgb16_in_argb32_over_argb32;
twin_in_op_func _twin_rgb16_in_rgb16_over_argb32;
twin_in_op_func _twin_rgb16_in_a8_over_argb32;
twin_in_op_func _twin_rgb16_in_c_over_argb32;
twin_in_op_func _twin_a8_in_argb32_over_argb32;
twin_in_op_func _twin_a8_in_rgb16_over_argb32;
twin_in_op_func _twin_a8_in_a8_over_argb32;
twin_in_op_func _twin_a8_in_c_over_argb32;
twin_in_op_func _twin_c_in_argb32_over_argb32;
twin_in_op_func _twin_c_in_rgb16_over_argb32;
twin_in_op_func _twin_c_in_a8_over_argb32;
twin_in_op_func _twin_c_in_c_over_argb32;
twin_in_op_func _twin_argb32_in_argb32_over_rgb16;
twin_in_op_func _twin_argb32_in_rgb16_over_rgb16;
twin_in_op_func _twin_argb32_in_a8_over_rgb16;
twin_in_op_func _twin_argb32_in_c_over_rgb16;
twin_in_op_func _twin_rgb16_in_argb32_over_rgb16;
twin_in_op_func _twin_rgb16_in_rgb16_over_rgb16;
twin_in_op_func _twin_rgb16_in_a8_over_rgb16;
twin_in_op_func _twin_rgb16_in_c_over_rgb16;
twin_in_op_func _twin_a8_in_argb32_over_rgb16;
twin_in_op_func _twin_a8_in_rgb16_over_rgb16;
twin_in_op_func _twin_a8_in_a8_over_rgb16;
twin_in_op_func _twin_a8_in_c_over_rgb16;
twin_in_op_func _twin_c_in_argb32_over_rgb16;
twin_in_op_func _twin_c_in_rgb16_over_rgb16;
twin_in_op_func _twin_c_in_a8_over_rgb16;
twin_in_op_func _twin_c_in_c_over_rgb16;
twin_in_op_func _twin_argb32_in_argb32_over_a8;
twin_in_op_func _twin_argb32_in_rgb16_over_a8;
twin_in_op_func _twin_argb32_in_a8_over_a8;
twin_in_op_func _twin_argb32_in_c_over_a8;
twin_in_op_func _twin_rgb16_in_argb32_over_a8;
twin_in_op_func _twin_rgb16_in_rgb16_over_a8;
twin_in_op_func _twin_rgb16_in_a8_over_a8;
twin_in_op_func _twin_rgb16_in_c_over_a8;
twin_in_op_func _twin_a8_in_argb32_over_a8;
twin_in_op_func _twin_a8_in_rgb16_over_a8;
twin_in_op_func _twin_a8_in_a8_over_a8;
twin_in_op_func _twin_a8_in_c_over_a8;
twin_in_op_func _twin_c_in_argb32_over_a8;
twin_in_op_func _twin_c_in_rgb16_over_a8;
twin_in_op_func _twin_c_in_a8_over_a8;
twin_in_op_func _twin_c_in_c_over_a8;
twin_in_op_func _twin_argb32_in_argb32_over_c;

twin_in_op_func _twin_argb32_in_argb32_source_argb32;
twin_in_op_func _twin_argb32_in_rgb16_source_argb32;
twin_in_op_func _twin_argb32_in_a8_source_argb32;
twin_in_op_func _twin_argb32_in_c_source_argb32;
twin_in_op_func _twin_rgb16_in_argb32_source_argb32;
twin_in_op_func _twin_rgb16_in_rgb16_source_argb32;
twin_in_op_func _twin_rgb16_in_a8_source_argb32;
twin_in_op_func _twin_rgb16_in_c_source_argb32;
twin_in_op_func _twin_a8_in_argb32_source_argb32;
twin_in_op_func _twin_a8_in_rgb16_source_argb32;
twin_in_op_func _twin_a8_in_a8_source_argb32;
twin_in_op_func _twin_a8_in_c_source_argb32;
twin_in_op_func _twin_c_in_argb32_source_argb32;
twin_in_op_func _twin_c_in_rgb16_source_argb32;
twin_in_op_func _twin_c_in_a8_source_argb32;
twin_in_op_func _twin_c_in_c_source_argb32;
twin_in_op_func _twin_argb32_in_argb32_source_rgb16;
twin_in_op_func _twin_argb32_in_rgb16_source_rgb16;
twin_in_op_func _twin_argb32_in_a8_source_rgb16;
twin_in_op_func _twin_argb32_in_c_source_rgb16;
twin_in_op_func _twin_rgb16_in_argb32_source_rgb16;
twin_in_op_func _twin_rgb16_in_rgb16_source_rgb16;
twin_in_op_func _twin_rgb16_in_a8_source_rgb16;
twin_in_op_func _twin_rgb16_in_c_source_rgb16;
twin_in_op_func _twin_a8_in_argb32_source_rgb16;
twin_in_op_func _twin_a8_in_rgb16_source_rgb16;
twin_in_op_func _twin_a8_in_a8_source_rgb16;
twin_in_op_func _twin_a8_in_c_source_rgb16;
twin_in_op_func _twin_c_in_argb32_source_rgb16;
twin_in_op_func _twin_c_in_rgb16_source_rgb16;
twin_in_op_func _twin_c_in_a8_source_rgb16;
twin_in_op_func _twin_c_in_c_source_rgb16;
twin_in_op_func _twin_argb32_in_argb32_source_a8;
twin_in_op_func _twin_argb32_in_rgb16_source_a8;
twin_in_op_func _twin_argb32_in_a8_source_a8;
twin_in_op_func _twin_argb32_in_c_source_a8;
twin_in_op_func _twin_rgb16_in_argb32_source_a8;
twin_in_op_func _twin_rgb16_in_rgb16_source_a8;
twin_in_op_func _twin_rgb16_in_a8_source_a8;
twin_in_op_func _twin_rgb16_in_c_source_a8;
twin_in_op_func _twin_a8_in_argb32_source_a8;
twin_in_op_func _twin_a8_in_rgb16_source_a8;
twin_in_op_func _twin_a8_in_a8_source_a8;
twin_in_op_func _twin_a8_in_c_source_a8;
twin_in_op_func _twin_c_in_argb32_source_a8;
twin_in_op_func _twin_c_in_rgb16_source_a8;
twin_in_op_func _twin_c_in_a8_source_a8;
twin_in_op_func _twin_c_in_c_source_a8;
twin_in_op_func _twin_argb32_in_argb32_source_c;

twin_op_func _twin_argb32_over_argb32;
twin_op_func _twin_rgb16_over_argb32;
twin_op_func _twin_a8_over_argb32;
twin_op_func _twin_c_over_argb32;
twin_op_func _twin_argb32_over_rgb16;
twin_op_func _twin_rgb16_over_rgb16;
twin_op_func _twin_a8_over_rgb16;
twin_op_func _twin_c_over_rgb16;
twin_op_func _twin_argb32_over_a8;
twin_op_func _twin_rgb16_over_a8;
twin_op_func _twin_a8_over_a8;
twin_op_func _twin_c_over_a8;
twin_op_func _twin_argb32_source_argb32;
twin_op_func _twin_rgb16_source_argb32;
twin_op_func _twin_a8_source_argb32;
twin_op_func _twin_c_source_argb32;
twin_op_func _twin_argb32_source_rgb16;
twin_op_func _twin_rgb16_source_rgb16;
twin_op_func _twin_a8_source_rgb16;
twin_op_func _twin_c_source_rgb16;
twin_op_func _twin_argb32_source_a8;
twin_op_func _twin_rgb16_source_a8;
twin_op_func _twin_a8_source_a8;
twin_op_func _twin_c_source_a8;

twin_argb32_t *
_twin_fetch_rgb16 (twin_pixmap_t *pixmap, int x, int y, int w, twin_argb32_t *span);

twin_argb32_t *
_twin_fetch_argb32 (twin_pixmap_t *pixmap, int x, int y, int w, twin_argb32_t *span);

/*
 * Geometry helper functions
 */

twin_dfixed_t
_twin_distance_to_point_squared (twin_spoint_t *a, twin_spoint_t *b);

twin_dfixed_t
_twin_distance_to_line_squared (twin_spoint_t *p, twin_spoint_t *p1, twin_spoint_t *p2);


/*
 * Polygon stuff
 */

/*
 * Fixed point helper functions
 */
twin_sfixed_t
_twin_sfixed_sqrt (twin_sfixed_t as);
    
/*
 * Matrix stuff
 */

twin_sfixed_t
_twin_matrix_x (twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y);

twin_sfixed_t
_twin_matrix_y (twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y);

twin_sfixed_t
_twin_matrix_dx (twin_matrix_t *m, twin_fixed_t dx, twin_fixed_t dy);

twin_sfixed_t
_twin_matrix_dy (twin_matrix_t *m, twin_fixed_t dx, twin_fixed_t dy);

twin_fixed_t
_twin_matrix_determinant (twin_matrix_t *matrix);

twin_sfixed_t
_twin_matrix_len (twin_matrix_t *m, twin_fixed_t dx, twin_fixed_t dy);

twin_point_t
_twin_matrix_expand (twin_matrix_t *matrix);
/*
 * Path stuff
 */

/*
 * A path
 */

twin_spoint_t
_twin_path_current_spoint (twin_path_t *path);

twin_spoint_t
_twin_path_subpath_first_spoint (twin_path_t *path);

void 
_twin_path_smove (twin_path_t *path, twin_sfixed_t x, twin_sfixed_t y);

void
_twin_path_sdraw (twin_path_t *path, twin_sfixed_t x, twin_sfixed_t y);

void
_twin_path_scurve (twin_path_t	    *path,
		   twin_sfixed_t    x1, twin_sfixed_t y1,
		   twin_sfixed_t    x2, twin_sfixed_t y2,
		   twin_sfixed_t    x3, twin_sfixed_t y3);

/*
 * Glyph stuff.  Coordinates are stored in 2.6 fixed point format
 */

#define TWIN_FONT_MAX	0x7f

extern const twin_gpoint_t	_twin_glyphs[];
extern const uint16_t		_twin_glyph_offsets[];

#define TWIN_GLYPH_MAX_POINTS	56

extern const signed char _twin_gtable[];
extern const uint16_t _twin_g_offsets[];

#define twin_glyph_left(g)	((g)[0])
#define twin_glyph_right(g)	((g)[1])
#define twin_glyph_ascent(g)	((g)[2])
#define twin_glyph_descent(g)	((g)[3])
#define twin_glyph_n_snap_x(g)	((g)[4])
#define twin_glyph_n_snap_y(g)	((g)[5])
#define twin_glyph_snap_x(g)	(&g[6])
#define twin_glyph_snap_y(g)	(twin_glyph_snap_x(g) + twin_glyph_n_snap_x(g))
#define twin_glyph_draw(g)	(twin_glyph_snap_y(g) + twin_glyph_n_snap_y(g))

#endif /* _TWININT_H_ */
