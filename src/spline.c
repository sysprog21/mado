/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Carl Worth <cworth@cworth.org>
 * All rights reserved.
 */

#include "twin_private.h"

typedef struct _twin_spline {
    twin_spoint_t a, b, c, d;
} twin_spline_t;

/*
 * Linearly interpolate between points 'a' and 'b' with a shift factor.
 * The shift factor determines the position between 'a' and 'b'.
 * The result is stored in 'result'.
 */
static void _lerp(twin_spoint_t *a,
                  twin_spoint_t *b,
                  int shift,
                  twin_spoint_t *result)
{
    result->x = a->x + ((b->x - a->x) >> shift);
    result->y = a->y + ((b->y - a->y) >> shift);
}

/*
 * Perform the de Casteljau algorithm to split a spline at a given shift
 * factor. The spline is split into two new splines 's1' and 's2'.
 */
static void _de_casteljau(twin_spline_t *spline,
                          int shift,
                          twin_spline_t *s1,
                          twin_spline_t *s2)
{
    twin_spoint_t ab, bc, cd;
    twin_spoint_t abbc, bccd;
    twin_spoint_t final;

    _lerp(&spline->a, &spline->b, shift, &ab);
    _lerp(&spline->b, &spline->c, shift, &bc);
    _lerp(&spline->c, &spline->d, shift, &cd);
    _lerp(&ab, &bc, shift, &abbc);
    _lerp(&bc, &cd, shift, &bccd);
    _lerp(&abbc, &bccd, shift, &final);

    s1->a = spline->a;
    s1->b = ab;
    s1->c = abbc;
    s1->d = final;

    s2->a = final;
    s2->b = bccd;
    s2->c = cd;
    s2->d = spline->d;
}

/*
 * Return an upper bound on the error (squared) that could result from
 * approximating a spline with a line segment connecting the two endpoints.
 */
static twin_dfixed_t _twin_spline_error_squared(twin_spline_t *spline)
{
    twin_dfixed_t berr, cerr;

    berr = _twin_distance_to_line_squared(&spline->b, &spline->a, &spline->d);
    cerr = _twin_distance_to_line_squared(&spline->c, &spline->a, &spline->d);

    if (berr > cerr)
        return berr;
    return cerr;
}

/*
 * Check if a spline is flat enough by comparing the error against the
 * tolerance.
 */
static bool is_flat(twin_spline_t *spline, twin_dfixed_t tolerance_squared)
{
    return _twin_spline_error_squared(spline) <= tolerance_squared;
}

/*
 * Decomposes a spline into a series of flat segments and draws them to a path.
 * Uses iterative approach to avoid deep recursion.
 * See https://keithp.com/blogs/iterative-splines/
 */
static void _twin_spline_decompose(twin_path_t *path,
                                   twin_spline_t *spline,
                                   twin_dfixed_t tolerance_squared)
{
    /* Draw starting point */
    _twin_path_sdraw(path, spline->a.x, spline->a.y);

    while (!is_flat(spline, tolerance_squared)) {
        int shift = 1;
        twin_spline_t left, right;

        /* FIXME: Find the optimal shift value to decompose the spline */
        do {
            _de_casteljau(spline, shift, &left, &right);
            shift++;
        } while (!is_flat(&left, tolerance_squared));

        /* Draw the left segment */
        _twin_path_sdraw(path, left.a.x, left.a.y);

        /* Update spline to the right segment */
        memcpy(spline, &right, sizeof(twin_spline_t));
    }

    /* Draw the ending point */
    _twin_path_sdraw(path, spline->d.x, spline->d.y);
}

void _twin_path_scurve(twin_path_t *path,
                       twin_sfixed_t x1,
                       twin_sfixed_t y1,
                       twin_sfixed_t x2,
                       twin_sfixed_t y2,
                       twin_sfixed_t x3,
                       twin_sfixed_t y3)
{
    if (path->npoints == 0)
        _twin_path_smove(path, 0, 0);

    twin_spline_t spline = {
        .a = path->points[path->npoints - 1],
        .b = {.x = x1, .y = y1},
        .c = {.x = x2, .y = y2},
        .d = {.x = x3, .y = y3},
    };
    _twin_spline_decompose(path, &spline,
                           TWIN_SFIXED_TOLERANCE * TWIN_SFIXED_TOLERANCE);
    _twin_path_sdraw(path, x3, y3);
}

void twin_path_curve(twin_path_t *path,
                     twin_fixed_t x1,
                     twin_fixed_t y1,
                     twin_fixed_t x2,
                     twin_fixed_t y2,
                     twin_fixed_t x3,
                     twin_fixed_t y3)
{
    return _twin_path_scurve(path, _twin_matrix_x(&path->state.matrix, x1, y1),
                             _twin_matrix_y(&path->state.matrix, x1, y1),
                             _twin_matrix_x(&path->state.matrix, x2, y2),
                             _twin_matrix_y(&path->state.matrix, x2, y2),
                             _twin_matrix_x(&path->state.matrix, x3, y3),
                             _twin_matrix_y(&path->state.matrix, x3, y3));
}
