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

#include <stdio.h>

#define F(x) twin_fixed_to_double(x)

#define S(x) (twin_int_to_fixed (x) / 2)

twin_bool_t
twin_has_glyph (int glyph)
{
    return _twin_font[glyph] != NULL;
}

void
twin_path_glyph (twin_path_t *path, int glyph)
{
    const twin_gpoint_t	*p = _twin_font[glyph];
    int			i;
    twin_fixed_t	xo, yo;
    twin_fixed_t	xc, yc;
    
    if (!p)
	return;
    
    twin_path_cur_point (path, &xo, &yo);
    twin_path_close (path);
    
    xc = xo - S (p[0].x);
    yc = yo - S (16);

    for (i = 1; p[i].y != -64; i++)
	if (p[i].x == -64)
	    twin_path_close (path);
	else
	    twin_path_draw (path, 
			    xc + S (p[i].x),
			    yc + S (p[i].y));

    xo = xo + twin_glyph_width (glyph);
    twin_path_move (path, xo, yo);
}

int
twin_glyph_width (int glyph)
{
    const twin_gpoint_t	*p = _twin_font[glyph];

    if (!p)
	return 0;
    
    return twin_fixed_ceil (S (p[0].y) - S (p[0].x));
}

extern const uint16_t _twin_unicode[];
    
void
twin_path_string (twin_path_t *path, unsigned char *string)
{
    
}

extern twin_font_t  twin_Bitstream_Vera_Sans_Roman;

#define gw(f)	((twin_fixed_t) ((twin_dfixed_t) (f) * scale_x) >> 6)
#define gx(f)	(((twin_fixed_t) ((twin_dfixed_t) (f) * scale_x) >> 6) + origin.x)
#define gy(f)	(-((twin_fixed_t) ((twin_dfixed_t) (f) * scale_y) >> 6) + origin.y)

static void
twin_path_fglyph (twin_path_t *path, 
		  twin_fixed_t scale_x, 
		  twin_fixed_t scale_y,
		  const char *glyph)
{
    twin_point_t    c, c1, c2, to;
    twin_point_t    from;
    twin_point_t    origin;
    twin_fixed_t    width;
    
    width = *glyph++;
    twin_path_cur_point (path, &origin.x, &origin.y);
    for (;;) {
	switch (*glyph++) {
	case 'm':
	    to.x = gx (*glyph++);
	    to.y = gy (*glyph++);
	    twin_path_move (path, to.x, to.y);
	    break;
	case 'l':
	    to.x = gx (*glyph++);
	    to.y = gy (*glyph++);
	    twin_path_draw (path, to.x, to.y);
	    break;
	case '2':
	    twin_path_cur_point (path, &from.x, &from.y);
	    c.x = gx (*glyph++);
	    c.y = gy (*glyph++);
	    to.x = gx (*glyph++);
	    to.y = gy (*glyph++);
	    c1.x = from.x + 2 * (c.x - from.x) / 3;
	    c1.y = from.y + 2 * (c.y - from.y) / 3;
	    c2.x = to.x + 2 * (c.x - to.x) / 3;
	    c2.y = to.y + 2 * (c.y - to.y) / 3;
	    twin_path_curve (path, c1.x, c1.y, c2.x, c2.y, to.x, to.y);
	    break;
	case '3':
	    c1.x = gx (*glyph++);
	    c1.y = gy (*glyph++);
	    c2.x = gx (*glyph++);
	    c2.y = gy (*glyph++);
	    to.x = gx (*glyph++);
	    to.y = gy (*glyph++);
	    twin_path_curve (path, c1.x, c1.y, c2.x, c2.y, to.x, to.y);
	    break;
	case 'e':
	    twin_path_move (path, gx(width), gy(0));
	    return;
	}
    }
}

void
twin_path_ucs4 (twin_path_t	*path, 
		twin_fixed_t	scale_x,
		twin_fixed_t	scale_y,
		twin_ucs4_t	ucs4)
{
    int		page = ucs4 >> TWIN_UCS_PAGE_SHIFT;
    int		off = ucs4 & (TWIN_UCS_PER_PAGE - 1);
    int		p;
    uint16_t	o;
    twin_font_t	*font = &twin_Bitstream_Vera_Sans_Roman;

    for (p = 0; p < font->ncharmap; p++)
	if (font->charmap[p].page == page)
	{
	    o = font->charmap[p].offsets[off];
	    if (o)
	    {
		twin_path_fglyph (path, scale_x, scale_y, font->outlines + o - 1);
	    }
	}
}

static int
_twin_utf8_to_ucs4 (const char	    *src_orig,
		    twin_ucs4_t	    *dst)
{
    const char	    *src = src_orig;
    char	    s;
    int		    extra;
    twin_ucs4_t	    result;

    s = *src++;
    if (!s)
	return 0;
    
    if (!(s & 0x80))
    {
	result = s;
	extra = 0;
    } 
    else if (!(s & 0x40))
    {
	return -1;
    }
    else if (!(s & 0x20))
    {
	result = s & 0x1f;
	extra = 1;
    }
    else if (!(s & 0x10))
    {
	result = s & 0xf;
	extra = 2;
    }
    else if (!(s & 0x08))
    {
	result = s & 0x07;
	extra = 3;
    }
    else if (!(s & 0x04))
    {
	result = s & 0x03;
	extra = 4;
    }
    else if ( ! (s & 0x02))
    {
	result = s & 0x01;
	extra = 5;
    }
    else
    {
	return -1;
    }
    
    while (extra--)
    {
	result <<= 6;
	s = *src++;
	if (!s)
	    return -1;
	
	if ((s & 0xc0) != 0x80)
	    return -1;
	
	result |= s & 0x3f;
    }
    *dst = result;
    return src - src_orig;
}

void
twin_path_utf8 (twin_path_t	*path, 
		twin_fixed_t    scale_x,
		twin_fixed_t    scale_y,
		const char	*string)
{
    int		len;
    twin_ucs4_t	ucs4;

    while ((len = _twin_utf8_to_ucs4(string, &ucs4)) > 0)
    {
	twin_path_ucs4 (path, scale_x, scale_y, ucs4);
	string += len;
    }
}
