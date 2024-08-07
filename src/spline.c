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
 * Linearly interpolate between points 'a' and 'b' with a 't' factor.
 * The 't' factor determines the position between 'a' and 'b'.
 * The result is stored in 'result'.
 */
static void _lerp(const twin_spoint_t *a,
                  const twin_spoint_t *b,
                  twin_dfixed_t t,
                  twin_spoint_t *result)
{
    result->x = a->x + twin_dfixed_to_sfixed(twin_dfixed_mul(
                           twin_sfixed_to_dfixed(b->x - a->x), t));
    result->y = a->y + twin_dfixed_to_sfixed(twin_dfixed_mul(
                           twin_sfixed_to_dfixed(b->y - a->y), t));
}

/*
 * Perform the de Casteljau algorithm to split a spline at a given 't'
 * factor. The spline is split into two new splines 's1' and 's2'.
 */
static void _de_casteljau(twin_spline_t *spline,
                          twin_dfixed_t t,
                          twin_spline_t *s1,
                          twin_spline_t *s2)
{
    twin_spoint_t ab, bc, cd, abbc, bccd, final;

    _lerp(&spline->a, &spline->b, t, &ab);
    _lerp(&spline->b, &spline->c, t, &bc);
    _lerp(&spline->c, &spline->d, t, &cd);
    _lerp(&ab, &bc, t, &abbc);
    _lerp(&bc, &cd, t, &bccd);
    _lerp(&abbc, &bccd, t, &final);

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
 * Return an upper bound on the distance (squared) that could result from
 * approximating a spline with a line segment connecting the two endpoints.
 */
static twin_dfixed_t _twin_spline_distance_squared(twin_spline_t *spline)
{
    twin_dfixed_t berr, cerr;

    berr = _twin_distance_to_line_squared(&spline->b, &spline->a, &spline->d);
    cerr = _twin_distance_to_line_squared(&spline->c, &spline->a, &spline->d);

    if (berr > cerr)
        return berr;
    return cerr;
}

/*
 * Check if a spline is flat enough by comparing the distance against the
 * tolerance.
 */
static bool is_flat(twin_spline_t *spline, twin_dfixed_t tolerance_squared)
{
    return _twin_spline_distance_squared(spline) <= tolerance_squared;
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
        twin_dfixed_t hi = TWIN_SFIXED_ONE * TWIN_SFIXED_ONE, lo = 0,
                      t_optimal = 0, t, max_distance = 0, distance;
        twin_spline_t left, right;

        while (true) {
            t = (hi + lo) >> 1;
            if (t_optimal == t)
                break;

            _de_casteljau(spline, t, &left, &right);

            distance = _twin_spline_distance_squared(&left);
            if (distance < max_distance)
                break;

            /* The left segment is close enough to fit the original spline. */
            if (distance <= tolerance_squared) {
                max_distance = distance;
                t_optimal = t;
                /*
                 * Try to find a better point such that the line segment
                 * connecting it to the previous point can fit the spline, while
                 * maximizing the distance from the spline as much as possible.
                 */
                lo = t;
            } else {
                /*
                 * The line segment connecting the previous point to the
                 * currently tested point with value 't' cannot fit the original
                 * spline. The point with value 't_optimal' is sufficient to fit
                 * the original spline.
                 */
                if (t_optimal)
                    break;
                hi = t;
            }
        }

        _de_casteljau(spline, t_optimal, &left, &right);

        /* Draw the left segment */
        _twin_path_sdraw(path, left.d.x, left.d.y);

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
