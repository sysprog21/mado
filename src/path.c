/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

static int _twin_current_subpath_len(twin_path_t *path)
{
    int start;

    if (path->nsublen)
        start = path->sublen[path->nsublen - 1];
    else
        start = 0;
    return path->npoints - start;
}

twin_spoint_t _twin_path_current_spoint(twin_path_t *path)
{
    if (!path->npoints)
        twin_path_move(path, 0, 0);
    return path->points[path->npoints - 1];
}

twin_spoint_t _twin_path_subpath_first_spoint(twin_path_t *path)
{
    int start;

    if (!path->npoints)
        twin_path_move(path, 0, 0);

    if (path->nsublen)
        start = path->sublen[path->nsublen - 1];
    else
        start = 0;

    return path->points[start];
}

void _twin_path_sfinish(twin_path_t *path)
{
    switch (_twin_current_subpath_len(path)) {
    case 1:
        path->npoints--;
    case 0:
        return;
    }

    if (path->nsublen == path->size_sublen) {
        int size_sublen;
        int *sublen;

        if (path->size_sublen > 0)
            size_sublen = path->size_sublen * 2;
        else
            size_sublen = 1;
        if (path->sublen)
            sublen = realloc(path->sublen, size_sublen * sizeof(int));
        else
            sublen = malloc(size_sublen * sizeof(int));
        if (!sublen)
            return;
        path->sublen = sublen;
        path->size_sublen = size_sublen;
    }
    path->sublen[path->nsublen] = path->npoints;
    path->nsublen++;
}

void _twin_path_smove(twin_path_t *path, twin_sfixed_t x, twin_sfixed_t y)
{
    switch (_twin_current_subpath_len(path)) {
    default:
        _twin_path_sfinish(path);
        fallthrough;
    case 0:
        _twin_path_sdraw(path, x, y);
        break;
    case 1:
        path->points[path->npoints - 1].x = x;
        path->points[path->npoints - 1].y = y;
        break;
    }
}

void _twin_path_sdraw(twin_path_t *path, twin_sfixed_t x, twin_sfixed_t y)
{
    if (_twin_current_subpath_len(path) > 0 &&
        path->points[path->npoints - 1].x == x &&
        path->points[path->npoints - 1].y == y)
        return;
    if (path->npoints == path->size_points) {
        int size_points;
        twin_spoint_t *points;

        if (path->size_points > 0)
            size_points = path->size_points * 2;
        else
            size_points = 16;
        if (path->points)
            points = realloc(path->points, size_points * sizeof(twin_spoint_t));
        else
            points = malloc(size_points * sizeof(twin_spoint_t));
        if (!points)
            return;
        path->points = points;
        path->size_points = size_points;
    }
    path->points[path->npoints].x = x;
    path->points[path->npoints].y = y;
    path->npoints++;
}

void twin_path_move(twin_path_t *path, twin_fixed_t x, twin_fixed_t y)
{
    _twin_path_smove(path, _twin_matrix_x(&path->state.matrix, x, y),
                     _twin_matrix_y(&path->state.matrix, x, y));
}

void twin_path_rmove(twin_path_t *path, twin_fixed_t dx, twin_fixed_t dy)
{
    twin_spoint_t here = _twin_path_current_spoint(path);
    _twin_path_smove(path,
                     here.x + _twin_matrix_dx(&path->state.matrix, dx, dy),
                     here.y + _twin_matrix_dy(&path->state.matrix, dx, dy));
}

void twin_path_draw(twin_path_t *path, twin_fixed_t x, twin_fixed_t y)
{
    _twin_path_sdraw(path, _twin_matrix_x(&path->state.matrix, x, y),
                     _twin_matrix_y(&path->state.matrix, x, y));
}

static void twin_path_draw_polar(twin_path_t *path, twin_angle_t deg)
{
    twin_fixed_t s, c;
    twin_sincos(deg, &s, &c);
    twin_path_draw(path, c, s);
}

void twin_path_rdraw(twin_path_t *path, twin_fixed_t dx, twin_fixed_t dy)
{
    twin_spoint_t here = _twin_path_current_spoint(path);
    _twin_path_sdraw(path,
                     here.x + _twin_matrix_dx(&path->state.matrix, dx, dy),
                     here.y + _twin_matrix_dy(&path->state.matrix, dx, dy));
}

void twin_path_close(twin_path_t *path)
{
    twin_spoint_t f;

    switch (_twin_current_subpath_len(path)) {
    case 0:
    case 1:
        break;
    default:
        f = _twin_path_subpath_first_spoint(path);
        _twin_path_sdraw(path, f.x, f.y);
        break;
    }
}

void twin_path_circle(twin_path_t *path,
                      twin_fixed_t x,
                      twin_fixed_t y,
                      twin_fixed_t radius)
{
    twin_path_ellipse(path, x, y, radius, radius);
}

void twin_path_ellipse(twin_path_t *path,
                       twin_fixed_t x,
                       twin_fixed_t y,
                       twin_fixed_t x_radius,
                       twin_fixed_t y_radius)
{
    twin_path_move(path, x + x_radius, y);
    twin_path_arc(path, x, y, x_radius, y_radius, 0, TWIN_ANGLE_360);
    twin_path_close(path);
}

static twin_fixed_t _twin_matrix_max_radius(twin_matrix_t *m)
{
    return (twin_fixed_abs(m->m[0][0]) + twin_fixed_abs(m->m[0][1]) +
            twin_fixed_abs(m->m[1][0]) + twin_fixed_abs(m->m[1][1]));
}

void twin_path_arc(twin_path_t *path,
                   twin_fixed_t x,
                   twin_fixed_t y,
                   twin_fixed_t x_radius,
                   twin_fixed_t y_radius,
                   twin_angle_t start,
                   twin_angle_t extent)
{
    twin_matrix_t save = twin_path_current_matrix(path);

    twin_path_translate(path, x, y);
    twin_path_scale(path, x_radius, y_radius);

    twin_fixed_t max_radius = _twin_matrix_max_radius(&path->state.matrix);
    int32_t sides = max_radius / twin_sfixed_to_fixed(TWIN_SFIXED_TOLERANCE);
    if (sides > 1024)
        sides = 1024;

    /* Calculate the nearest power of 2 that is >= sides. */
    int32_t n = (sides > 1) ? (31 - twin_clz(sides) + 1) : 2;

    twin_angle_t step = TWIN_ANGLE_360 >> n;
    twin_angle_t inc = step;
    twin_angle_t epsilon = 1;
    if (extent < 0) {
        inc = -inc;
        epsilon = -1;
    }

    twin_angle_t first = (start + inc - epsilon) & ~(step - 1);
    twin_angle_t last = (start + extent - inc + epsilon) & ~(step - 1);

    if (first != start)
        twin_path_draw_polar(path, start);

    for (twin_angle_t a = first; a != last; a += inc)
        twin_path_draw_polar(path, a);

    if (last != start + extent)
        twin_path_draw_polar(path, start + extent);

    twin_path_set_matrix(path, save);
}

static twin_angle_t vector_angle(twin_fixed_t ux,
                                 twin_fixed_t uy,
                                 twin_fixed_t vx,
                                 twin_fixed_t vy)
{
    twin_fixed_t dot = twin_fixed_mul(ux, vx) + twin_fixed_mul(uy, vy);

    twin_fixed_t ua =
        twin_fixed_sqrt(twin_fixed_mul(ux, ux) + twin_fixed_mul(uy, uy));
    twin_fixed_t va =
        twin_fixed_sqrt(twin_fixed_mul(vx, vx) + twin_fixed_mul(vy, vy));

    /* cos(theta) = (u ⋅ v) / (|u| * |v|) */
    twin_fixed_t cos_theta = twin_fixed_div(dot, twin_fixed_mul(ua, va));
    twin_fixed_t cross = twin_fixed_mul(ux, vy) - twin_fixed_mul(uy, vx);
    twin_angle_t angle = twin_acos(cos_theta);
    return (cross < 0) ? -angle : angle;
}

typedef struct {
    twin_fixed_t cx, cy;
    twin_angle_t start, extent;
} twin_ellipse_param_t;

static twin_ellipse_param_t get_center_parameters(twin_fixed_t x1,
                                                  twin_fixed_t y1,
                                                  twin_fixed_t x2,
                                                  twin_fixed_t y2,
                                                  bool fa,
                                                  bool fs,
                                                  twin_fixed_t rx,
                                                  twin_fixed_t ry,
                                                  twin_angle_t phi)
{
    twin_fixed_t sin_phi, cos_phi;
    twin_sincos(phi, &sin_phi, &cos_phi);
    /* Simplify through translation/rotation */
    twin_fixed_t x =
        twin_fixed_mul(cos_phi, twin_fixed_mul(x1 - x2, TWIN_FIXED_HALF)) +
        twin_fixed_mul(sin_phi, twin_fixed_mul(y1 - y2, TWIN_FIXED_HALF));

    twin_fixed_t y =
        twin_fixed_mul(-sin_phi, twin_fixed_mul(x1 - x2, TWIN_FIXED_HALF)) +
        twin_fixed_mul(cos_phi, twin_fixed_mul(y1 - y2, TWIN_FIXED_HALF));

    twin_xfixed_t x_x = twin_fixed_to_xfixed(x);
    twin_xfixed_t y_x = twin_fixed_to_xfixed(y);
    twin_xfixed_t rx_x = twin_fixed_to_xfixed(rx);
    twin_xfixed_t ry_x = twin_fixed_to_xfixed(ry);
    twin_xfixed_t px_x = twin_xfixed_mul(x_x, x_x);
    twin_xfixed_t py_x = twin_xfixed_mul(y_x, y_x);
    twin_xfixed_t prx_x = twin_xfixed_mul(rx_x, rx_x);
    twin_xfixed_t pry_x = twin_xfixed_mul(ry_x, ry_x);
    twin_xfixed_t p_ry_divided_rx_x = twin_xfixed_div(pry_x, prx_x);
    twin_xfixed_t p_rx_divided_ry_x = twin_xfixed_div(prx_x, pry_x);
    /* Correct out-of-range radii */
    twin_fixed_t L = twin_xfixed_to_fixed(twin_xfixed_div(px_x, prx_x) +
                                          twin_xfixed_div(py_x, pry_x));
    if (L > TWIN_FIXED_ONE) {
        twin_fixed_t sqrt_L = twin_fixed_sqrt(L);
        rx = twin_fixed_mul(sqrt_L, twin_fixed_abs(rx));
        ry = twin_fixed_mul(sqrt_L, twin_fixed_abs(ry));
    } else {
        rx = twin_fixed_abs(rx);
        ry = twin_fixed_abs(ry);
    }

    /* Compute center */
    twin_fixed_t sign = (fa != fs) ? -1 : 1;
    /* A = (ry^2)/(y^2 + (ry^2/rx^2)．x^2) */
    twin_xfixed_t A =
        twin_xfixed_div(pry_x, py_x + twin_xfixed_mul(p_ry_divided_rx_x, px_x));
    /* B = (y^2)/(y^2 + (ry^2/rx^2)．x^2) */
    twin_xfixed_t B =
        twin_xfixed_div(py_x, py_x + twin_xfixed_mul(p_ry_divided_rx_x, px_x));
    /* C = (x^2)/(x^2 + (rx^2/ry^2)．y^2) */
    twin_xfixed_t C =
        twin_xfixed_div(px_x, px_x + twin_xfixed_mul(p_rx_divided_ry_x, py_x));
    twin_xfixed_t pM = A - B - C;
    twin_fixed_t M = sign * twin_xfixed_to_fixed(_twin_xfixed_sqrt(pM));
    twin_fixed_t _cx =
        twin_fixed_mul(M, twin_fixed_div(twin_fixed_mul(rx, y), ry));
    twin_fixed_t _cy =
        twin_fixed_mul(M, twin_fixed_div(twin_fixed_mul(-ry, x), rx));

    twin_ellipse_param_t ret;
    ret.cx = twin_fixed_mul(cos_phi, _cx) - twin_fixed_mul(sin_phi, _cy) +
             twin_fixed_mul(x1 + x2, TWIN_FIXED_HALF);

    ret.cy = twin_fixed_mul(sin_phi, _cx) + twin_fixed_mul(cos_phi, _cy) +
             twin_fixed_mul(y1 + y2, TWIN_FIXED_HALF);

    /* Compute θ and dθ */
    ret.start = vector_angle(TWIN_FIXED_ONE, 0, twin_fixed_div(x - _cx, rx),
                             twin_fixed_div(y - _cy, ry));
    twin_angle_t extent = vector_angle(
        twin_fixed_div(x - _cx, rx), twin_fixed_div(y - _cy, ry),
        twin_fixed_div(-x - _cx, rx), twin_fixed_div(-y - _cy, ry));

    if (fs && extent > TWIN_ANGLE_0)
        extent -= TWIN_ANGLE_360;
    if (!fs && extent < TWIN_ANGLE_0)
        extent += TWIN_ANGLE_360;
    ret.start %= TWIN_ANGLE_360;
    extent %= TWIN_ANGLE_360;

    ret.extent = extent;
    return ret;
}

void twin_path_arc_ellipse(twin_path_t *path,
                           bool large_arc,
                           bool sweep,
                           twin_fixed_t radius_x,
                           twin_fixed_t radius_y,
                           twin_fixed_t cur_x,
                           twin_fixed_t cur_y,
                           twin_fixed_t target_x,
                           twin_fixed_t target_y,
                           twin_angle_t rotation)
{
    twin_ellipse_param_t param;
    param = get_center_parameters(cur_x, cur_y, target_x, target_y, large_arc,
                                  sweep, radius_x, radius_y, rotation);
    twin_matrix_t save = twin_path_current_matrix(path);

    twin_path_translate(path, param.cx, param.cy);
    twin_path_rotate(path, rotation);
    twin_path_translate(path, -param.cx, -param.cy);
    twin_path_arc(path, param.cx, param.cy, radius_x, radius_y, param.start,
                  param.extent);

    twin_path_set_matrix(path, save);
}

void twin_path_arc_circle(twin_path_t *path,
                          bool large_arc,
                          bool sweep,
                          twin_fixed_t radius,
                          twin_fixed_t cur_x,
                          twin_fixed_t cur_y,
                          twin_fixed_t target_x,
                          twin_fixed_t target_y)
{
    twin_path_arc_ellipse(path, large_arc, sweep, radius, radius, cur_x, cur_y,
                          target_x, target_y, TWIN_ANGLE_0);
}

void twin_path_rectangle(twin_path_t *path,
                         twin_fixed_t x,
                         twin_fixed_t y,
                         twin_fixed_t w,
                         twin_fixed_t h)
{
    twin_path_move(path, x, y);
    twin_path_draw(path, x + w, y);
    twin_path_draw(path, x + w, y + h);
    twin_path_draw(path, x, y + h);
    twin_path_close(path);
}

void twin_path_rounded_rectangle(twin_path_t *path,
                                 twin_fixed_t x,
                                 twin_fixed_t y,
                                 twin_fixed_t w,
                                 twin_fixed_t h,
                                 twin_fixed_t x_radius,
                                 twin_fixed_t y_radius)
{
    twin_matrix_t save = twin_path_current_matrix(path);

    twin_path_translate(path, x, y);
    twin_path_move(path, 0, y_radius);
    twin_path_arc(path, x_radius, y_radius, x_radius, y_radius, TWIN_ANGLE_180,
                  TWIN_ANGLE_90);
    twin_path_draw(path, w - x_radius, 0);
    twin_path_arc(path, w - x_radius, y_radius, x_radius, y_radius,
                  TWIN_ANGLE_270, TWIN_ANGLE_90);
    twin_path_draw(path, w, h - y_radius);
    twin_path_arc(path, w - x_radius, h - y_radius, x_radius, y_radius,
                  TWIN_ANGLE_0, TWIN_ANGLE_90);
    twin_path_draw(path, x_radius, h);
    twin_path_arc(path, x_radius, h - y_radius, x_radius, y_radius,
                  TWIN_ANGLE_90, TWIN_ANGLE_90);
    twin_path_close(path);
    twin_path_set_matrix(path, save);
}

void twin_path_lozenge(twin_path_t *path,
                       twin_fixed_t x,
                       twin_fixed_t y,
                       twin_fixed_t w,
                       twin_fixed_t h)
{
    twin_fixed_t radius;

    if (w > h)
        radius = h / 2;
    else
        radius = w / 2;
    twin_path_rounded_rectangle(path, x, y, w, h, radius, radius);
}

void twin_path_tab(twin_path_t *path,
                   twin_fixed_t x,
                   twin_fixed_t y,
                   twin_fixed_t w,
                   twin_fixed_t h,
                   twin_fixed_t x_radius,
                   twin_fixed_t y_radius)
{
    twin_matrix_t save = twin_path_current_matrix(path);

    twin_path_translate(path, x, y);
    twin_path_move(path, 0, y_radius);
    twin_path_arc(path, x_radius, y_radius, x_radius, y_radius, TWIN_ANGLE_180,
                  TWIN_ANGLE_90);
    twin_path_draw(path, w - x_radius, 0);
    twin_path_arc(path, w - x_radius, y_radius, x_radius, y_radius,
                  TWIN_ANGLE_270, TWIN_ANGLE_90);
    twin_path_draw(path, w, h);
    twin_path_draw(path, 0, h);
    twin_path_close(path);
    twin_path_set_matrix(path, save);
}

void twin_path_set_matrix(twin_path_t *path, twin_matrix_t matrix)
{
    path->state.matrix = matrix;
}

twin_matrix_t twin_path_current_matrix(twin_path_t *path)
{
    return path->state.matrix;
}

void twin_path_identity(twin_path_t *path)
{
    twin_matrix_identity(&path->state.matrix);
}

void twin_path_translate(twin_path_t *path, twin_fixed_t tx, twin_fixed_t ty)
{
    twin_matrix_translate(&path->state.matrix, tx, ty);
}

void twin_path_scale(twin_path_t *path, twin_fixed_t sx, twin_fixed_t sy)
{
    twin_matrix_scale(&path->state.matrix, sx, sy);
}

void twin_path_rotate(twin_path_t *path, twin_angle_t a)
{
    twin_matrix_rotate(&path->state.matrix, a);
}

void twin_path_set_font_size(twin_path_t *path, twin_fixed_t font_size)
{
    path->state.font_size = font_size;
}

twin_fixed_t twin_path_current_font_size(twin_path_t *path)
{
    return path->state.font_size;
}

void twin_path_set_font_style(twin_path_t *path, twin_style_t font_style)
{
    path->state.font_style = font_style;
}

twin_style_t twin_path_current_font_style(twin_path_t *path)
{
    return path->state.font_style;
}

void twin_path_set_cap_style(twin_path_t *path, twin_cap_t cap_style)
{
    path->state.cap_style = cap_style;
}

twin_cap_t twin_path_current_cap_style(twin_path_t *path)
{
    return path->state.cap_style;
}

void twin_path_empty(twin_path_t *path)
{
    path->npoints = 0;
    path->nsublen = 0;
}

void twin_path_bounds(twin_path_t *path, twin_rect_t *rect)
{
    twin_sfixed_t left = TWIN_SFIXED_MAX;
    twin_sfixed_t top = TWIN_SFIXED_MAX;
    twin_sfixed_t right = TWIN_SFIXED_MIN;
    twin_sfixed_t bottom = TWIN_SFIXED_MIN;
    int i;

    for (i = 0; i < path->npoints; i++) {
        twin_sfixed_t x = path->points[i].x;
        twin_sfixed_t y = path->points[i].y;
        if (x < left)
            left = x;
        if (x > right)
            right = x;
        if (y < top)
            top = y;
        if (y > bottom)
            bottom = y;
    }
    if (left >= right || top >= bottom)
        left = right = top = bottom = 0;
    rect->left = twin_sfixed_trunc(left);
    rect->top = twin_sfixed_trunc(top);
    rect->right = twin_sfixed_trunc(twin_sfixed_ceil(right));
    rect->bottom = twin_sfixed_trunc(twin_sfixed_ceil(bottom));
}

void twin_path_append(twin_path_t *dst, twin_path_t *src)
{
    for (int p = 0, s = 0; p < src->npoints; p++) {
        if (s < src->nsublen && p == src->sublen[s]) {
            _twin_path_sfinish(dst);
            s++;
        }
        _twin_path_sdraw(dst, src->points[p].x, src->points[p].y);
    }
}

twin_state_t twin_path_save(twin_path_t *path)
{
    return path->state;
}

void twin_path_restore(twin_path_t *path, twin_state_t *state)
{
    path->state = *state;
}

twin_path_t *twin_path_create(void)
{
    twin_path_t *path;

    path = malloc(sizeof(twin_path_t));
    path->npoints = path->size_points = 0;
    path->nsublen = path->size_sublen = 0;
    path->points = 0;
    path->sublen = 0;
    twin_matrix_identity(&path->state.matrix);
    path->state.font_size = TWIN_FIXED_ONE * 15;
    path->state.font_style = TwinStyleRoman;
    path->state.cap_style = TwinCapRound;
    return path;
}

void twin_path_destroy(twin_path_t *path)
{
    free(path->points);
    free(path->sublen);
    free(path);
}

void twin_composite_path(twin_pixmap_t *dst,
                         twin_operand_t *src,
                         twin_coord_t src_x,
                         twin_coord_t src_y,
                         twin_path_t *path,
                         twin_operator_t operator)
{
    twin_rect_t bounds;
    twin_path_bounds(path, &bounds);
    if (bounds.left >= bounds.right || bounds.top >= bounds.bottom)
        return;

    twin_coord_t width = bounds.right - bounds.left;
    twin_coord_t height = bounds.bottom - bounds.top;
    twin_pixmap_t *mask = twin_pixmap_create(TWIN_A8, width, height);
    if (!mask)
        return;

    twin_fill_path(mask, path, -bounds.left, -bounds.top);
    twin_operand_t msk = {.source_kind = TWIN_PIXMAP, .u.pixmap = mask};
    twin_composite(dst, bounds.left, bounds.top, src, src_x + bounds.left,
                   src_y + bounds.top, &msk, 0, 0, operator, width, height);
    twin_pixmap_destroy(mask);
}

void twin_paint_path(twin_pixmap_t *dst, twin_argb32_t argb, twin_path_t *path)
{
    twin_operand_t src = {.source_kind = TWIN_SOLID, .u.argb = argb};
    twin_composite_path(dst, &src, 0, 0, path, TWIN_OVER);
}

void twin_composite_stroke(twin_pixmap_t *dst,
                           twin_operand_t *src,
                           twin_coord_t src_x,
                           twin_coord_t src_y,
                           twin_path_t *stroke,
                           twin_fixed_t pen_width,
                           twin_operator_t operator)
{
    twin_path_t *pen = twin_path_create();
    twin_path_t *path = twin_path_create();
    twin_matrix_t m = twin_path_current_matrix(stroke);

    m.m[2][0] = 0;
    m.m[2][1] = 0;
    twin_path_set_matrix(pen, m);
    twin_path_set_cap_style(path, twin_path_current_cap_style(stroke));
    twin_path_circle(pen, 0, 0, pen_width / 2);
    twin_path_convolve(path, stroke, pen);
    twin_composite_path(dst, src, src_x, src_y, path, operator);
    twin_path_destroy(path);
    twin_path_destroy(pen);
}

void twin_paint_stroke(twin_pixmap_t *dst,
                       twin_argb32_t argb,
                       twin_path_t *stroke,
                       twin_fixed_t pen_width)
{
    twin_operand_t src = {.source_kind = TWIN_SOLID, .u.argb = argb};
    twin_composite_stroke(dst, &src, 0, 0, stroke, pen_width, TWIN_OVER);
}
