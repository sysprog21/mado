/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Carl Worth <cworth@cworth.org>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include "twin_private.h"

typedef struct _twin_spline {
    twin_spoint_t a, b, c, d;
} twin_spline_t;

/*
 * Linearly interpolate between points 'a' and 'b' with a 'shift' factor.
 * The 'shift' factor determines the position between 'a' and 'b'.
 * The result is stored in 'result'.
 */
static void _lerp(const twin_spoint_t *a,
                  const twin_spoint_t *b,
                  int shift,
                  twin_spoint_t *result)
{
    result->x = a->x + ((b->x - a->x) >> shift);
    result->y = a->y + ((b->y - a->y) >> shift);
}

/*
 * Perform the de Casteljau algorithm to split a spline at a given 'shift'
 * factor. The spline is split into two new splines 's1' and 's2'.
 */
static void _de_casteljau(twin_spline_t *spline,
                          int shift,
                          twin_spline_t *s1,
                          twin_spline_t *s2)
{
    twin_spoint_t ab, bc, cd, abbc, bccd, final;

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
 * Return an upper bound on the distance (squared) that could result from
 * approximating a spline with a line segment connecting the two endpoints,
 * which is based on the Convex Hull Property of Bézier Curves: The Bézier Curve
 * lies completely in the convex hull of the given control points. Therefore, we
 * can use control points B and C to approximate the actual spline.
 */
static twin_dfixed_t _twin_spline_distance_squared(twin_spline_t *spline)
{
    twin_dfixed_t bdist, cdist;

    bdist = _twin_distance_to_line_squared(&spline->b, &spline->a, &spline->d);
    cdist = _twin_distance_to_line_squared(&spline->c, &spline->a, &spline->d);

    if (bdist > cdist)
        return bdist;
    return cdist;
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
    /*
     * It on average requires over two shift attempts per iteration to find the
     * optimal value. To reduce redundancy in shift 1, adjust the initial 't'
     * value from 0.5 to 0.25 by applying an initial shift of 2. As spline
     * rendering progresses, the shift amount decreases. Store the last shift
     * value as a global variable to use directly in the next iteration,
     * avoiding a reset to an initial shift of 2.
     */
    int shift = 2;
    while (!is_flat(spline, tolerance_squared)) {
        twin_spline_t left, right;

        while (true) {
            _de_casteljau(spline, shift, &left, &right);
            if (is_flat(&left, tolerance_squared)) {
                /* Limiting the scope of 't' may overlook optimal points with
                 * maximum curvature. Therefore, dynamically reduce the shift
                 * amount to a minimum of 1. */
                if (shift > 1)
                    shift--;
                break;
            }
            shift++;
        }

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

void twin_path_quadratic_curve(twin_path_t *path,
                               twin_fixed_t x1,
                               twin_fixed_t y1,
                               twin_fixed_t x2,
                               twin_fixed_t y2)
{
    /* Convert quadratic to cubic control point */
    twin_spoint_t p0 = _twin_path_current_spoint(path);
    twin_sfixed_t x1s = _twin_matrix_x(&path->state.matrix, x1, y1);
    twin_sfixed_t y1s = _twin_matrix_y(&path->state.matrix, x1, y1);
    twin_sfixed_t x2s = _twin_matrix_x(&path->state.matrix, x2, y2);
    twin_sfixed_t y2s = _twin_matrix_y(&path->state.matrix, x2, y2);
    /* CP1 = P0 + 2/3 * (P1 - P0) */
    twin_sfixed_t dx1 = x1s - p0.x;
    twin_sfixed_t dy1 = y1s - p0.y;
    twin_sfixed_t cx1 =
        p0.x + twin_sfixed_mul(twin_double_to_sfixed(2.0 / 3.0), dx1);
    twin_sfixed_t cy1 =
        p0.y + twin_sfixed_mul(twin_double_to_sfixed(2.0 / 3.0), dy1);
    /* CP2 = P2 + 2/3 * (P1 - P2) */
    twin_sfixed_t dx2 = x1s - x2s;
    twin_sfixed_t dy2 = y1s - y2s;
    twin_sfixed_t cx2 =
        x2s + twin_sfixed_mul(twin_double_to_sfixed(2.0 / 3.0), dx2);
    twin_sfixed_t cy2 =
        y2s + twin_sfixed_mul(twin_double_to_sfixed(2.0 / 3.0), dy2);
    _twin_path_scurve(path, cx1, cy1, cx2, cy2, x2s, y2s);
}
