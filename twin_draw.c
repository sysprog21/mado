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

#include "twinint.h"

/* op, src, dst */
static twin_src_op  comp2[2][4][3] = {
    {	/* OVER */
	{   /* A8 */
	    _twin_a8_over_a8,
	    _twin_a8_over_rgb16,
	    _twin_a8_over_argb32,
	},
	{   /* RGB16 */
	    _twin_rgb16_over_a8,
	    _twin_rgb16_over_rgb16,
	    _twin_rgb16_over_argb32,
	},
	{   /* ARGB32 */
	    _twin_argb32_over_a8,
	    _twin_argb32_over_rgb16,
	    _twin_argb32_over_argb32,
	},
	{   /* C */
	    _twin_c_over_a8,
	    _twin_c_over_rgb16,
	    _twin_c_over_argb32,
	}
    },
    {	/* SOURCE */
	{   /* A8 */
	    _twin_a8_source_a8,
	    _twin_a8_source_rgb16,
	    _twin_a8_source_argb32,
	},
	{   /* RGB16 */
	    _twin_rgb16_source_a8,
	    _twin_rgb16_source_rgb16,
	    _twin_rgb16_source_argb32,
	},
	{   /* ARGB32 */
	    _twin_argb32_source_a8,
	    _twin_argb32_source_rgb16,
	    _twin_argb32_source_argb32,
	},
	{   /* C */
	    _twin_c_source_a8,
	    _twin_c_source_rgb16,
	    _twin_c_source_argb32,
	}
    }
};

/* op, src, msk, dst */
static twin_src_msk_op	comp3[2][4][4][3] = {
    {	/* OVER */
	{   /* A8 */
	    {	/* A8 */
		_twin_a8_in_a8_over_a8,
		_twin_a8_in_a8_over_rgb16,
		_twin_a8_in_a8_over_argb32,
	    },
	    {	/* RGB16 */
		_twin_a8_in_rgb16_over_a8,
		_twin_a8_in_rgb16_over_rgb16,
		_twin_a8_in_rgb16_over_argb32,
	    },
	    {	/* ARGB32 */
		_twin_a8_in_argb32_over_a8,
		_twin_a8_in_argb32_over_rgb16,
		_twin_a8_in_argb32_over_argb32,
	    },
	    {	/* C */
		_twin_a8_in_c_over_a8,
		_twin_a8_in_c_over_rgb16,
		_twin_a8_in_c_over_argb32,
	    },
	},
	{   /* RGB16 */
	    {	/* A8 */
		_twin_rgb16_in_a8_over_a8,
		_twin_rgb16_in_a8_over_rgb16,
		_twin_rgb16_in_a8_over_argb32,
	    },
	    {	/* RGB16 */
		_twin_rgb16_in_rgb16_over_a8,
		_twin_rgb16_in_rgb16_over_rgb16,
		_twin_rgb16_in_rgb16_over_argb32,
	    },
	    {	/* ARGB32 */
		_twin_rgb16_in_argb32_over_a8,
		_twin_rgb16_in_argb32_over_rgb16,
		_twin_rgb16_in_argb32_over_argb32,
	    },
	    {	/* C */
		_twin_rgb16_in_c_over_a8,
		_twin_rgb16_in_c_over_rgb16,
		_twin_rgb16_in_c_over_argb32,
	    },
	},
	{   /* ARGB32 */
	    {	/* A8 */
		_twin_argb32_in_a8_over_a8,
		_twin_argb32_in_a8_over_rgb16,
		_twin_argb32_in_a8_over_argb32,
	    },
	    {	/* RGB16 */
		_twin_argb32_in_rgb16_over_a8,
		_twin_argb32_in_rgb16_over_rgb16,
		_twin_argb32_in_rgb16_over_argb32,
	    },
	    {	/* ARGB32 */
		_twin_argb32_in_argb32_over_a8,
		_twin_argb32_in_argb32_over_rgb16,
		_twin_argb32_in_argb32_over_argb32,
	    },
	    {	/* C */
		_twin_argb32_in_c_over_a8,
		_twin_argb32_in_c_over_rgb16,
		_twin_argb32_in_c_over_argb32,
	    },
	},
	{   /* C */
	    {	/* A8 */
		_twin_c_in_a8_over_a8,
		_twin_c_in_a8_over_rgb16,
		_twin_c_in_a8_over_argb32,
	    },
	    {	/* RGB16 */
		_twin_c_in_rgb16_over_a8,
		_twin_c_in_rgb16_over_rgb16,
		_twin_c_in_rgb16_over_argb32,
	    },
	    {	/* ARGB32 */
		_twin_c_in_argb32_over_a8,
		_twin_c_in_argb32_over_rgb16,
		_twin_c_in_argb32_over_argb32,
	    },
	    {	/* C */
		_twin_c_in_c_over_a8,
		_twin_c_in_c_over_rgb16,
		_twin_c_in_c_over_argb32,
	    },
	},
    },
    {	/* SOURCE */
	{   /* A8 */
	    {	/* A8 */
		_twin_a8_in_a8_source_a8,
		_twin_a8_in_a8_source_rgb16,
		_twin_a8_in_a8_source_argb32,
	    },
	    {	/* RGB16 */
		_twin_a8_in_rgb16_source_a8,
		_twin_a8_in_rgb16_source_rgb16,
		_twin_a8_in_rgb16_source_argb32,
	    },
	    {	/* ARGB32 */
		_twin_a8_in_argb32_source_a8,
		_twin_a8_in_argb32_source_rgb16,
		_twin_a8_in_argb32_source_argb32,
	    },
	    {	/* C */
		_twin_a8_in_c_source_a8,
		_twin_a8_in_c_source_rgb16,
		_twin_a8_in_c_source_argb32,
	    },
	},
	{   /* RGB16 */
	    {	/* A8 */
		_twin_rgb16_in_a8_source_a8,
		_twin_rgb16_in_a8_source_rgb16,
		_twin_rgb16_in_a8_source_argb32,
	    },
	    {	/* RGB16 */
		_twin_rgb16_in_rgb16_source_a8,
		_twin_rgb16_in_rgb16_source_rgb16,
		_twin_rgb16_in_rgb16_source_argb32,
	    },
	    {	/* ARGB32 */
		_twin_rgb16_in_argb32_source_a8,
		_twin_rgb16_in_argb32_source_rgb16,
		_twin_rgb16_in_argb32_source_argb32,
	    },
	    {	/* C */
		_twin_rgb16_in_c_source_a8,
		_twin_rgb16_in_c_source_rgb16,
		_twin_rgb16_in_c_source_argb32,
	    },
	},
	{   /* ARGB32 */
	    {	/* A8 */
		_twin_argb32_in_a8_source_a8,
		_twin_argb32_in_a8_source_rgb16,
		_twin_argb32_in_a8_source_argb32,
	    },
	    {	/* RGB16 */
		_twin_argb32_in_rgb16_source_a8,
		_twin_argb32_in_rgb16_source_rgb16,
		_twin_argb32_in_rgb16_source_argb32,
	    },
	    {	/* ARGB32 */
		_twin_argb32_in_argb32_source_a8,
		_twin_argb32_in_argb32_source_rgb16,
		_twin_argb32_in_argb32_source_argb32,
	    },
	    {	/* C */
		_twin_argb32_in_c_source_a8,
		_twin_argb32_in_c_source_rgb16,
		_twin_argb32_in_c_source_argb32,
	    },
	},
	{   /* C */
	    {	/* A8 */
		_twin_c_in_a8_source_a8,
		_twin_c_in_a8_source_rgb16,
		_twin_c_in_a8_source_argb32,
	    },
	    {	/* RGB16 */
		_twin_c_in_rgb16_source_a8,
		_twin_c_in_rgb16_source_rgb16,
		_twin_c_in_rgb16_source_argb32,
	    },
	    {	/* ARGB32 */
		_twin_c_in_argb32_source_a8,
		_twin_c_in_argb32_source_rgb16,
		_twin_c_in_argb32_source_argb32,
	    },
	    {	/* C */
		_twin_c_in_c_source_a8,
		_twin_c_in_c_source_rgb16,
		_twin_c_in_c_source_argb32,
	    },
	},
    }
};

    
#define operand_index(o)    ((o)->source_kind == TWIN_SOLID ? 3 : o->u.pixmap->format)

void
twin_composite (twin_pixmap_t	*dst,
		twin_coord_t	dst_x,
		twin_coord_t	dst_y,
		twin_operand_t	*src,
		twin_coord_t	src_x,
		twin_coord_t	src_y,
		twin_operand_t	*msk,
		twin_coord_t	msk_x,
		twin_coord_t	msk_y,
		twin_operator_t	operator,
		twin_coord_t	width,
		twin_coord_t	height)
{
    twin_coord_t    iy;
    twin_coord_t    left, right, top, bottom;

    left = dst_x;
    right = dst_x + width;
    top = dst_y;
    bottom = dst_y + height;
    if (left < 0)
	left = 0;
    if (right > dst->width)
	right = dst->width;
    if (top < 0)
	top = 0;
    if (bottom > dst->height)
	bottom = dst->height;
    if (left >= right || top >= bottom)
	return;

    twin_pixmap_lock (dst);
    if (msk)
    {
	twin_src_msk_op	op;
	twin_source_u   s, m;
	twin_coord_t	sdx, sdy, mdx, mdy;
	
	sdx = src_x - dst_x;
	sdy = src_y - dst_y;
	mdx = msk_x - dst_x;
	mdy = msk_y - dst_y;
	

	op = comp3[operator][operand_index(src)][operand_index(msk)][dst->format];
	if (op)
	{
	    if (src->source_kind == TWIN_SOLID)
		s.c = src->u.argb;
	    if (msk->source_kind == TWIN_SOLID)
		s.c = msk->u.argb;
	    for (iy = top; iy < bottom; iy++)
	    {
		if (src->source_kind == TWIN_PIXMAP)
		    s.p = twin_pixmap_pointer (src->u.pixmap, left+sdx, iy+sdy);
		if (msk->source_kind == TWIN_PIXMAP)
		    m.p = twin_pixmap_pointer (msk->u.pixmap, left+mdx, iy+mdy);
		(*op) (twin_pixmap_pointer (dst, left, iy),
		       s, m, right - left);
	    }
	}
	else
	{
	}
    }
    else
    {
	twin_src_op	op;
	twin_source_u   s;
	twin_coord_t	sdx, sdy;
	
	sdx = src_x - dst_x;
	sdy = src_y - dst_y;
	
	op = comp2[operator][operand_index(src)][dst->format];
        if (src->source_kind == TWIN_SOLID)
	    s.c = src->u.argb;
	for (iy = top; iy < bottom; iy++)
	{
	    if (src->source_kind == TWIN_PIXMAP)
		s.p = twin_pixmap_pointer (src->u.pixmap, left+sdx, iy+sdy);
	    (*op) (twin_pixmap_pointer (dst, left, iy),
		   s, right - left);
	}
    }
    twin_pixmap_damage (dst, left, top, right, bottom);
    twin_pixmap_unlock (dst);
}
	     
/*
 * array primary    index is OVER SOURCE
 * array secondary  index is ARGB32 RGB16 A8
 */
static twin_src_op  fill[2][3] = {
    {	/* OVER */
	_twin_c_over_a8,
	_twin_c_over_rgb16,
	_twin_c_over_argb32,
    },
    {	/* SOURCE */
	_twin_c_source_a8,
	_twin_c_source_rgb16,
	_twin_c_source_argb32,
    }
};

void
twin_fill (twin_pixmap_t    *dst,
	   twin_argb32_t    pixel,
	   twin_operator_t  operator,
	   twin_coord_t	    left,
	   twin_coord_t	    top,
	   twin_coord_t	    right,
	   twin_coord_t	    bottom)
{
    twin_src_op	    op;
    twin_source_u   src;
    twin_coord_t    iy;
    
    twin_pixmap_lock (dst);
    src.c = pixel;
    if (left < 0)
	left = 0;
    if (right > dst->width)
	right = dst->width;
    if (top < 0)
	top = 0;
    if (bottom > dst->height)
	bottom = dst->height;
    op = fill[operator][dst->format];
    for (iy = top; iy < bottom; iy++)
	(*op) (twin_pixmap_pointer (dst, left, iy), src, right - left);
    twin_pixmap_damage (dst, left, right, top, bottom);
    twin_pixmap_unlock (dst);
}
