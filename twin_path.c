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

static int
_twin_current_subpath_len (twin_path_t *path)
{
    int	start;
    
    if (path->nsublen)
	start = path->sublen[path->nsublen-1];
    else
	start = 0;
    return path->npoints - start;
}

twin_spoint_t
_twin_path_current_spoint (twin_path_t *path)
{
    if (!path->npoints)
	twin_path_move (path, 0, 0);
    return path->points[path->npoints - 1];
}

twin_spoint_t
_twin_path_subpath_first_spoint (twin_path_t *path)
{
    int	start;
    
    if (!path->npoints)
	twin_path_move (path, 0, 0);

    if (path->nsublen)
	start = path->sublen[path->nsublen-1];
    else
	start = 0;
    
    return path->points[start];
}

void 
_twin_path_smove (twin_path_t *path, twin_sfixed_t x, twin_sfixed_t y)
{
    switch (_twin_current_subpath_len (path)) {
    default:
	twin_path_close (path);
    case 0:
	_twin_path_sdraw (path, x, y);
	break;
    case 1:
	path->points[path->npoints-1].x = x;
	path->points[path->npoints-1].y = y;
	break;
    }
}

void
_twin_path_sdraw (twin_path_t *path, twin_sfixed_t x, twin_sfixed_t y)
{
    if (_twin_current_subpath_len(path) > 0 &&
	path->points[path->npoints-1].x == x &&
	path->points[path->npoints-1].y == y)
	return;
    if (path->npoints == path->size_points)
    {
	int		size_points;
	twin_spoint_t	*points;
	
	if (path->size_points > 0)
	    size_points = path->size_points * 2;
	else
	    size_points = 16;
	if (path->points)
	    points = realloc (path->points, size_points * sizeof (twin_spoint_t));
	else
	    points = malloc (size_points * sizeof (twin_spoint_t));
	if (!points)
	    return;
	path->points = points;
	path->size_points = size_points;
    }
    path->points[path->npoints].x = x;
    path->points[path->npoints].y = y;
    path->npoints++;
}

void
twin_path_move (twin_path_t *path, twin_fixed_t x, twin_fixed_t y)
{
    _twin_path_smove (path, 
		      _twin_matrix_x (&path->state.matrix, x, y),
		      _twin_matrix_y (&path->state.matrix, x, y));
}

void
twin_path_draw (twin_path_t *path, twin_fixed_t x, twin_fixed_t y)
{
    _twin_path_sdraw (path, 
		      _twin_matrix_x (&path->state.matrix, x, y),
		      _twin_matrix_y (&path->state.matrix, x, y));
}

void
twin_path_close (twin_path_t *path)
{
    switch (_twin_current_subpath_len(path)) {
    case 1:
	path->npoints--;
    case 0:
	return;
    }
    
    if (path->nsublen == path->size_sublen)
    {
	int	size_sublen;
	int	*sublen;
	
	if (path->size_sublen > 0)
	    size_sublen = path->size_sublen * 2;
	else
	    size_sublen = 16;
	if (path->sublen)
	    sublen = realloc (path->sublen, size_sublen * sizeof (int));
	else
	    sublen = malloc (size_sublen * sizeof (int));
	if (!sublen)
	    return;
	path->sublen = sublen;
	path->size_sublen = size_sublen;
    }
    path->sublen[path->nsublen] = path->npoints;
    path->nsublen++;
}

void
twin_path_circle (twin_path_t *path, twin_fixed_t radius)
{
    int		    sides;
    int		    n;
    twin_spoint_t   center;
    int		    i;
    twin_matrix_t   save;
    twin_fixed_t    det;

    save = twin_path_current_matrix (path);

    twin_path_scale (path, radius, radius);

    center = _twin_path_current_spoint (path);

    twin_path_close (path);
    
    /* The determinant represents the area expansion factor of the
       transform. In the worst case, this is entirely in one
       dimension, which is what we assume here. */

    det = _twin_matrix_determinant (&path->state.matrix);
    
    sides = (4 * det) / twin_sfixed_to_fixed (TWIN_SFIXED_TOLERANCE);
    
    n = 2;
    while ((1 << n) < sides)
	n++;

    for (i = 0; i <= (1 << n); i++)
    {
	twin_angle_t	a = (i * TWIN_ANGLE_360) >> n;
	twin_fixed_t	x = twin_cos (a);
	twin_fixed_t	y = twin_sin (a);

	_twin_path_sdraw (path, 
			  center.x + _twin_matrix_dx (&path->state.matrix, x, y),
			  center.y + _twin_matrix_dy (&path->state.matrix, x, y));
    }
    
    twin_path_close (path);
    twin_path_set_matrix (path, save);
}

void
twin_path_set_matrix (twin_path_t *path, twin_matrix_t matrix)
{
    path->state.matrix = matrix;
}

twin_matrix_t
twin_path_current_matrix (twin_path_t *path)
{
    return path->state.matrix;
}

void
twin_path_identity (twin_path_t *path)
{
    twin_matrix_identity (&path->state.matrix);
}

void
twin_path_translate (twin_path_t *path, twin_fixed_t tx, twin_fixed_t ty)
{
    twin_matrix_translate (&path->state.matrix, tx, ty);
}

void
twin_path_scale (twin_path_t *path, twin_fixed_t sx, twin_fixed_t sy)
{
    twin_matrix_scale (&path->state.matrix, sx, sy);
}
    
void
twin_path_rotate (twin_path_t *path, twin_angle_t a)
{
    twin_matrix_rotate (&path->state.matrix, a);
}
    
void
twin_path_set_font_size (twin_path_t *path, twin_fixed_t font_size)
{
    path->state.font_size = font_size;
}

twin_fixed_t
twin_path_current_font_size (twin_path_t *path)
{
    return path->state.font_size;
}

void
twin_path_set_font_style (twin_path_t *path, int font_style)
{
    path->state.font_style = font_style;
}

int
twin_path_current_font_style (twin_path_t *path)
{
    return path->state.font_style;
}

void
twin_path_empty (twin_path_t *path)
{
    path->npoints = 0;
    path->nsublen = 0;
}

void
twin_path_append (twin_path_t *dst, twin_path_t *src)
{
    int	    p;
    int	    s = 0;

    for (p = 0; p < src->npoints; p++)
    {
	if (s < src->nsublen && p == src->sublen[s])
	{
	    twin_path_close (dst);
	    s++;
	}
	_twin_path_sdraw (dst, src->points[p].x, src->points[p].y);
    }
}

twin_state_t
twin_path_save (twin_path_t *path)
{
    return path->state;
}

void
twin_path_restore (twin_path_t *path, twin_state_t *state)
{
    path->state = *state;
}

twin_path_t *
twin_path_create (void)
{
    twin_path_t	*path;

    path = malloc (sizeof (twin_path_t));
    path->npoints = path->size_points = 0;
    path->nsublen = path->size_sublen = 0;
    path->points = 0;
    path->sublen = 0;
    twin_matrix_identity (&path->state.matrix);
    path->state.font_size = TWIN_FIXED_ONE * 15;
    path->state.font_style = TWIN_TEXT_ROMAN;
    return path;
}

void
twin_path_destroy (twin_path_t *path)
{
    if (path->points)
	free (path->points);
    if (path->sublen)
	free (path->sublen);
    free (path);
}
