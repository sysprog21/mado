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
    ((t) = twin_int_mult(twin_get_8(d,i),(m),(t)) + twin_get_8(s,i),\
     (twin_argb32_t) twin_sat (t) << (i))

#define twin_over(s,d,i,m,t)	\
    ((t) = twin_int_mult(twin_get_8(d,i),(m),(t)) + twin_get_8(s,i),\
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
_twin_distance_to_point_squared (twin_point_t *a, twin_point_t *b);

twin_dfixed_t
_twin_distance_to_line_squared (twin_point_t *p, twin_point_t *p1, twin_point_t *p2);


/*
 * Polygon stuff
 */

typedef struct _twin_edge {
    struct _twin_edge	*next;
    twin_fixed_t	top, bot;
    twin_fixed_t	x;
    twin_fixed_t	e;
    twin_fixed_t	dx, dy;
    twin_fixed_t	inc_x;
    twin_fixed_t	step_x;
    int			winding;
} twin_edge_t;

/*
 * Pixmap must be in a8 format.
 */

int
_twin_edge_build (twin_point_t *vertices, int nvertices, twin_edge_t *edges);

void
_twin_edge_fill (twin_pixmap_t *pixmap, twin_edge_t *edges, int nedges);

/*
 * Path stuff
 */

/*
 * A (fixed point) path
 */

struct _twin_path {
    twin_point_t    *points;
    int		    size_points;
    int		    npoints;
    int		    *sublen;
    int		    size_sublen;
    int		    nsublen;
};

#endif /* _TWININT_H_ */
