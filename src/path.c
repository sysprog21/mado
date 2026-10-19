/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
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
        int size_sublen = path->size_sublen * 2;
        int *sublen;

        if (path->sublen == path->inline_sublen) {
            /* Outgrowing inline storage */
            sublen = twin_malloc(size_sublen * sizeof(int));
            if (!sublen)
                return;
            memcpy(sublen, path->inline_sublen, path->nsublen * sizeof(int));
        } else {
            sublen = twin_realloc(path->sublen, size_sublen * sizeof(int));
            if (!sublen)
                return;
        }
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
        int size_points = path->size_points * 2;
        twin_spoint_t *points;

        if (path->points == path->inline_points) {
            /* Outgrowing inline storage -- first heap allocation */
            points = twin_malloc(size_points * sizeof(twin_spoint_t));
            if (!points)
                return;
            memcpy(points, path->inline_points,
                   path->npoints * sizeof(twin_spoint_t));
        } else {
            points =
                twin_realloc(path->points, size_points * sizeof(twin_spoint_t));
            if (!points)
                return;
        }
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

/* pi sqrt(8) in 16.16: sides per square root of the radius in pixels */
#define ARC_SIDES_PER_ROOT ((twin_fixed_t) 582338)

/* An upper bound on how far @m moves a unit vector: the largest
 * singular value never exceeds the largest absolute row or column sum
 * (it is at most the geometric mean of the two norms).  Exact for an
 * axis-aligned scale, so an uneven ellipse gets its long radius. */
static twin_fixed_t _twin_matrix_max_radius(twin_matrix_t *m)
{
    twin_fixed_t a = twin_fixed_abs(m->m[0][0]), b = twin_fixed_abs(m->m[0][1]);
    twin_fixed_t c = twin_fixed_abs(m->m[1][0]), d = twin_fixed_abs(m->m[1][1]);
    twin_fixed_t r = max(a + b, c + d);
    r = max(r, a + c);
    return max(r, b + d);
}

void twin_path_arc(twin_path_t *path,
                   twin_fixed_t x,
                   twin_fixed_t y,
                   twin_fixed_t x_radius,
                   twin_fixed_t y_radius,
                   twin_angle_t start,
                   twin_angle_t extent)
{
    twin_matrix_t save = path->state.matrix;

    twin_path_translate(path, x, y);
    twin_path_scale(path, x_radius, y_radius);

    /* A chord of an n-gon strays r (1 - cos(pi / n)) ~ r pi^2 / (2 n^2)
     * from the circle, so a deviation of at most 1/16 pixel (a quarter of
     * the rasterizer's sample spacing) needs n = pi sqrt(8 r) sides. */
    twin_fixed_t max_radius = _twin_matrix_max_radius(&path->state.matrix);
    twin_fixed_t root = twin_fixed_sqrt(max_radius);
    int32_t sides =
        twin_fixed_to_int(twin_fixed_mul(root, ARC_SIDES_PER_ROOT)) + 1;
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
    twin_matrix_t save = path->state.matrix;

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
    twin_matrix_t save = path->state.matrix;

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

void twin_path_set_matrix(twin_path_t *path, twin_matrix_t matrix)
{
    path->state.matrix = matrix;
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

void twin_path_set_font_style(twin_path_t *path, twin_style_t font_style)
{
    path->state.font_style = font_style;
}

void twin_path_set_cap_style(twin_path_t *path, twin_cap_t cap_style)
{
    path->state.cap_style = cap_style;
}

void twin_path_set_fill_rule(twin_path_t *path, twin_fill_rule_t rule)
{
    path->state.fill_rule = rule;
}

void twin_path_empty(twin_path_t *path)
{
    path->npoints = 0;
    path->nsublen = 0;
}

static void _twin_path_reset_state(twin_path_t *path)
{
    twin_path_empty(path);
    twin_matrix_identity(&path->state.matrix);
    path->state.font = NULL;
    path->state.font_size = TWIN_FIXED_ONE * 15;
    path->state.font_style = TwinStyleRoman;
    path->state.cap_style = TwinCapRound;
    path->state.fill_rule = TwinFillNonZero;
}

void twin_path_set_font(twin_path_t *path, twin_font_t *font)
{
    path->state.font = font;
}

twin_path_t *_twin_path_acquire(twin_screen_t *screen)
{
    if (screen && screen->path_cache_count > 0) {
        twin_path_t *path = screen->path_cache[--screen->path_cache_count];
        _twin_path_reset_state(path);
        return path;
    }
    return twin_path_create();
}

void _twin_path_release(twin_screen_t *screen, twin_path_t *path)
{
    if (!path)
        return;

    /* Shrink oversized heap arrays back to inline storage */
    if (path->points != path->inline_points &&
        path->size_points > TWIN_PATH_INLINE_POINTS * 8) {
        twin_free(path->points);
        path->points = path->inline_points;
        path->size_points = TWIN_PATH_INLINE_POINTS;
    }
    if (path->sublen != path->inline_sublen &&
        path->size_sublen > TWIN_PATH_INLINE_SUBLEN * 8) {
        twin_free(path->sublen);
        path->sublen = path->inline_sublen;
        path->size_sublen = TWIN_PATH_INLINE_SUBLEN;
    }

    twin_path_empty(path);

    if (screen && screen->path_cache_count < TWIN_PATH_CACHE_SIZE) {
        screen->path_cache[screen->path_cache_count++] = path;
        return;
    }
    twin_path_destroy(path);
}

void twin_path_bounds(twin_path_t *path, twin_rect_t *rect)
{
    _twin_path_bounds_from(path, 0, rect);
}

void _twin_path_bounds_from(const twin_path_t *path,
                            int first,
                            twin_rect_t *rect)
{
    twin_sfixed_t left = TWIN_SFIXED_MAX;
    twin_sfixed_t top = TWIN_SFIXED_MAX;
    twin_sfixed_t right = TWIN_SFIXED_MIN;
    twin_sfixed_t bottom = TWIN_SFIXED_MIN;

    for (int i = first; i < path->npoints; i++) {
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

void _twin_path_init(twin_path_t *path)
{
    path->points = path->inline_points;
    path->size_points = TWIN_PATH_INLINE_POINTS;
    path->sublen = path->inline_sublen;
    path->size_sublen = TWIN_PATH_INLINE_SUBLEN;
    _twin_path_reset_state(path);
}

void _twin_path_fini(twin_path_t *path)
{
    if (path->points != path->inline_points)
        twin_free(path->points);
    if (path->sublen != path->inline_sublen)
        twin_free(path->sublen);
}

twin_path_t *twin_path_create(void)
{
    twin_path_t *path = twin_malloc(sizeof(twin_path_t));
    if (path)
        _twin_path_init(path);
    return path;
}

void twin_path_destroy(twin_path_t *path)
{
    _twin_path_fini(path);
    twin_free(path);
}

/*
 * Coverage for @path on @dst.
 *
 * On return @mask describes A8 coverage whose (0, 0) sits at (*mx, *my) in
 * path coordinates, and @vis is the part of the path bounds inside the
 * destination clip.  Small shapes come from (or go into) the coverage cache
 * as full-bounds masks; large ones are rasterized only where visible.
 * Returns false when nothing is visible or memory ran out.
 */
typedef struct {
    twin_pixmap_t hdr;   /* mask pixmap header (no allocation) */
    twin_pixmap_t *heap; /* heap mask to release, if any */
    bool scratch;        /* mask lives in the scratch arena */
    size_t scratch_saved;
} twin_mask_t;

static uint64_t _twin_path_key(const twin_path_t *path, const twin_rect_t *b)
{
    uint64_t h = TWIN_HASH_INIT ^ 0x50; /* 'P': path domain */
    twin_sfixed_t ox = twin_int_to_sfixed(b->left);
    twin_sfixed_t oy = twin_int_to_sfixed(b->top);
    for (int i = 0; i < path->npoints; i++) {
        twin_sfixed_t xy[2] = {(twin_sfixed_t) (path->points[i].x - ox),
                               (twin_sfixed_t) (path->points[i].y - oy)};
        h = _twin_hash(h, xy, sizeof(xy));
    }
    h = _twin_hash(h, path->sublen, sizeof(int) * path->nsublen);
    h = _twin_hash(h, &path->npoints, sizeof(path->npoints));
    return h;
}

static bool _twin_path_mask(twin_pixmap_t *dst,
                            twin_path_t *path,
                            twin_scratch_t *scratch,
                            twin_mask_t *m,
                            twin_coord_t *mx,
                            twin_coord_t *my,
                            twin_rect_t *vis,
                            bool *banded)
{
    twin_rect_t b;
    twin_path_bounds(path, &b);
    if (b.left >= b.right || b.top >= b.bottom)
        return false;

    twin_rect_t c = twin_pixmap_get_clip(dst); /* in path coordinates */
    *vis = (twin_rect_t) {max(b.left, c.left), min(b.right, c.right),
                          max(b.top, c.top), min(b.bottom, c.bottom)};
    if (vis->left >= vis->right || vis->top >= vis->bottom)
        return false;

    m->heap = NULL;
    m->scratch = false;
    twin_coord_t w = b.right - b.left, h = b.bottom - b.top;
    size_t full = (size_t) _twin_a8_stride(w) * (size_t) h;

    if (full <= CONFIG_CACHE_MAX_ENTRY && _twin_cache_enabled()) {
        uint64_t key = _twin_path_key(path, &b);
        twin_cache_entry_t *e = _twin_cache_lookup(key);
        bool fresh = false, collide = false;
        /* A 64-bit hash collision must never hand back a mask of another
         * size: that would read outside it.  Nor may it insert a duplicate
         * key, which lookups could never reach: rasterize uncached. */
        if (e && (e->width != w || e->height != h)) {
            e = NULL;
            collide = true;
        }
        if (!e && !collide) {
            e = _twin_cache_insert(key, w, h);
            fresh = e != NULL;
        }
        if (e) {
            _twin_pixmap_init_a8(&m->hdr, e->width, e->height, e->stride,
                                 e->data);
            if (fresh)
                twin_fill_path(&m->hdr, path, -b.left, -b.top, scratch);
            *mx = b.left;
            *my = b.top;
            return true;
        }
    }

    /* Rasterize only the visible part */
    w = vis->right - vis->left;
    h = vis->bottom - vis->top;
    twin_coord_t stride = _twin_a8_stride(w);
    size_t bytes = (size_t) stride * (size_t) h;
    void *pixels = NULL;
    if (scratch) {
        m->scratch_saved = twin_scratch_save(scratch);
        pixels = twin_scratch_alloc(scratch, bytes, 4);
        m->scratch = pixels != NULL;
    }
    if (pixels) {
        memset(pixels, 0, bytes);
        _twin_pixmap_init_a8(&m->hdr, w, h, stride, pixels);
    } else if (banded) {
        /* Larger than the scratch arena: the caller rasterizes in bands */
        *banded = true;
        return false;
    } else {
        /* Larger than the scratch arena: a heap mask for this call only, so
         * an idle screen holds no mask memory */
        twin_pixmap_t *mask = twin_pixmap_create(TWIN_A8, w, h);
        if (!mask)
            return false;
        m->heap = mask;
        for (twin_coord_t y = 0; y < h; y++)
            memset(twin_pixmap_pointer(mask, 0, y).v, 0, (size_t) w);
        _twin_pixmap_init_a8(&m->hdr, w, h, mask->stride, mask->p.v);
    }
    twin_fill_path(&m->hdr, path, -vis->left, -vis->top, scratch);
    *mx = vis->left;
    *my = vis->top;
    return true;
}

static void _twin_mask_release(twin_mask_t *m, twin_scratch_t *scratch)
{
    twin_pixmap_reset_xform_cache(&m->hdr);
    if (m->scratch)
        twin_scratch_restore(scratch, m->scratch_saved);
    if (m->heap)
        twin_pixmap_destroy(m->heap);
}

/* Bytes of mask a band may take: half the scratch arena, leaving room for
 * the edges, or a small heap buffer without one */
#define TWIN_BAND_HEAP_BYTES 4096

/*
 * Composite @path over the visible rectangle @vis one band of rows at a
 * time, so a shape larger than the scratch arena needs a band-sized mask
 * rather than a heap mask of its full size.  Edges that start above a band
 * are stepped to it exactly, so the coverage matches a single pass.
 */
typedef struct {
    twin_pixmap_t *dst, *band;
    twin_operand_t *src;
    twin_coord_t src_x, src_y;
    twin_operator_t operator;
    twin_rect_t vis;
} _band_composite_t;

static void _band_composite(void *ctx, twin_coord_t top, twin_coord_t rows)
{
    _band_composite_t *c = ctx;
    twin_coord_t y = c->vis.top + top;
    twin_operand_t msk = {.source_kind = TWIN_PIXMAP, .u.pixmap = c->band};
    twin_composite(c->dst, c->vis.left, y, c->src, c->src_x + c->vis.left,
                   c->src_y + y, &msk, 0, 0, c->operator,
                   c->vis.right - c->vis.left, rows);
    twin_pixmap_reset_xform_cache(c->band);
}

/*
 * Composite @path over the visible rectangle @vis one band of rows at a
 * time, so a shape larger than the scratch arena needs a band-sized mask
 * rather than a heap mask of its full size.  The edges are built and
 * sorted once; the scan hands each band over as it leaves it.
 */
static void _twin_composite_path_bands(twin_pixmap_t *dst,
                                       twin_operand_t *src,
                                       twin_coord_t src_x,
                                       twin_coord_t src_y,
                                       twin_path_t *path,
                                       twin_operator_t operator,
                                       twin_scratch_t * scratch,
                                       twin_rect_t vis)
{
    twin_coord_t w = vis.right - vis.left, h = vis.bottom - vis.top;
    twin_coord_t stride = _twin_a8_stride(w);
    size_t budget = scratch ? scratch->size / 2 : TWIN_BAND_HEAP_BYTES;
    twin_coord_t rows = (twin_coord_t) (budget / (size_t) stride);
    if (rows < 1)
        rows = 1;
    if (rows > h)
        rows = h;
    size_t bytes = (size_t) stride * (size_t) rows;

    size_t saved = scratch ? twin_scratch_save(scratch) : 0;
    uint8_t *buf = scratch ? twin_scratch_alloc(scratch, bytes, 4) : NULL;
    uint8_t *heap = NULL;
    if (!buf) {
        heap = twin_malloc(bytes);
        if (!heap)
            return;
        buf = heap;
    }
    memset(buf, 0, bytes);
    twin_pixmap_t band;
    _twin_pixmap_init_a8(&band, w, rows, stride, buf);
    _band_composite_t c = {dst, &band, src, src_x, src_y, operator, vis };
    twin_band_t bands = {h, _band_composite, &c};
    twin_fill_path_banded(&band, &bands, path, -vis.left, -vis.top, scratch);
    if (scratch)
        twin_scratch_restore(scratch, saved);
    twin_free(heap);
}

void twin_composite_path(twin_pixmap_t *dst,
                         twin_operand_t *src,
                         twin_coord_t src_x,
                         twin_coord_t src_y,
                         twin_path_t *path,
                         twin_operator_t operator,
                         twin_scratch_t * scratch)
{
    twin_mask_t m;
    twin_coord_t mx, my;
    twin_rect_t vis;
    bool banded = false;

    if (!_twin_path_mask(dst, path, scratch, &m, &mx, &my, &vis, &banded)) {
        if (banded)
            _twin_composite_path_bands(dst, src, src_x, src_y, path, operator,
                                       scratch, vis);
        return;
    }
    twin_operand_t msk = {.source_kind = TWIN_PIXMAP, .u.pixmap = &m.hdr};
    twin_composite(dst, vis.left, vis.top, src, src_x + vis.left,
                   src_y + vis.top, &msk, vis.left - mx, vis.top - my, operator,
                   vis.right - vis.left, vis.bottom - vis.top);
    _twin_mask_release(&m, scratch);
}

/* The screen's scratch arena for drawing on @dst, or NULL off screen */
static twin_scratch_t *_twin_screen_scratch(twin_pixmap_t *dst,
                                            twin_scratch_t *s)
{
    if (!dst->screen || !dst->screen->scratch_buf)
        return NULL;
    twin_scratch_init(s, dst->screen->scratch_buf, dst->screen->scratch_size);
    return s;
}

static twin_argb32_t _twin_lerp_argb(twin_argb32_t c0,
                                     twin_argb32_t c1,
                                     twin_fixed_t t)
{
    if (t <= 0)
        return c0;
    if (t >= TWIN_FIXED_ONE)
        return c1;
    /* |b - a| * t < 2^24: 32-bit products suffice */
    twin_argb32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        int32_t a = (c0 >> shift) & 0xff, b = (c1 >> shift) & 0xff;
        int32_t v = a + (((b - a) * t + 0x8000) >> 16);
        out |= (twin_argb32_t) (v & 0xff) << shift;
    }
    return out;
}

/* Floor division stepped by a constant: q + r / d advances by dq + dr / d
 * per step, with 0 <= r, dr < d */
typedef struct {
    int64_t q, r, dq, dr, d;
} _twin_step_t;

static void _twin_floor_divmod(int64_t n, int64_t d, int64_t *q, int64_t *r)
{
    *q = n / d;
    *r = n % d;
    if (*r < 0) {
        *r += d;
        (*q)--;
    }
}

/* Start at @n / @d, stepping by @step / @d; @d > 0 */
static _twin_step_t _twin_step_init(int64_t n, int64_t step, int64_t d)
{
    _twin_step_t t = {.d = d};
    _twin_floor_divmod(n, d, &t.q, &t.r);
    _twin_floor_divmod(step, d, &t.dq, &t.dr);
    return t;
}

static inline void _twin_step(_twin_step_t *t)
{
    t->q += t->dq;
    t->r += t->dr;
    if (t->r >= t->d) {
        t->r -= t->d;
        t->q++;
    }
}

/* Gradient color at 16.16 position @q, clamped to the ends */
static inline twin_argb32_t _twin_lerp_q(twin_argb32_t c0,
                                         twin_argb32_t c1,
                                         int64_t q)
{
    return q <= 0                ? c0
           : q >= TWIN_FIXED_ONE ? c1
                                 : _twin_lerp_argb(c0, c1, (twin_fixed_t) q);
}

void twin_paint_path_linear(twin_pixmap_t *dst,
                            twin_path_t *path,
                            twin_fixed_t x0,
                            twin_fixed_t y0,
                            twin_argb32_t c0,
                            twin_fixed_t x1,
                            twin_fixed_t y1,
                            twin_argb32_t c1)
{
    twin_scratch_t scratch;
    twin_scratch_t *sp = _twin_screen_scratch(dst, &scratch);

    twin_mask_t m;
    twin_coord_t mx, my;
    twin_rect_t vis;
    if (!_twin_path_mask(dst, path, sp, &m, &mx, &my, &vis, NULL))
        return;

    twin_operand_t msk = {.source_kind = TWIN_PIXMAP, .u.pixmap = &m.hdr};
    twin_coord_t w = vis.right - vis.left;
    int64_t gx = (int64_t) x1 - x0, gy = (int64_t) y1 - y0;
    int64_t len2 = (gx * gx + gy * gy) >> 16; /* 16.16 */

    if (gx == 0 || len2 == 0) {
        /* Vertical (or degenerate): one solid color per scanline.  t is
         * (cy - y0) / gy; a row down adds one pixel to cy, so t steps by a
         * fixed quotient and remainder.  With the signs folded into a
         * positive divisor, floor and truncation agree wherever t > 0. */
        int64_t d = gy < 0 ? -gy : gy;
        int64_t sign = gy < 0 ? -1 : 1;
        _twin_step_t t = {0, 0, 0, 0, d ? d : 1};
        if (gy) {
            twin_fixed_t cy = twin_int_to_fixed(vis.top) + TWIN_FIXED_HALF;
            t = _twin_step_init(sign * ((int64_t) cy - y0) * 65536,
                                sign * ((int64_t) TWIN_FIXED_ONE << 16), d);
        }
        for (twin_coord_t y = vis.top; y < vis.bottom; y++) {
            twin_operand_t src = {.source_kind = TWIN_SOLID,
                                  .u.argb = _twin_lerp_q(c0, c1, t.q)};
            twin_composite(dst, vis.left, y, &src, 0, 0, &msk, vis.left - mx,
                           y - my, TWIN_OVER, w, 1);
            _twin_step(&t); /* steps by nothing when gy is 0 */
        }
        _twin_mask_release(&m, sp);
        return;
    }

    /* General direction: build each row's colors, then composite it */
    twin_argb32_t *row = twin_malloc(sizeof(twin_argb32_t) * (size_t) w);
    if (!row) {
        _twin_mask_release(&m, sp);
        return;
    }
    twin_pixmap_t span;
    _twin_pixmap_init(&span, TWIN_ARGB32, w, 1, (twin_coord_t) (w * 4), row);
    twin_operand_t src = {.source_kind = TWIN_PIXMAP, .u.pixmap = &span};

    /* t = dot * 65536 / len2, floored.  One pixel to the right adds
     * exactly gx to dot and one row down adds exactly gy, so t is stepped
     * as a quotient and remainder both ways: no 64-bit division per row or
     * pixel on 32-bit targets.  Floor and truncating division differ only
     * for t <= 0, which selects c0 either way. */
    twin_fixed_t cx = twin_int_to_fixed(vis.left) + TWIN_FIXED_HALF;
    twin_fixed_t cy = twin_int_to_fixed(vis.top) + TWIN_FIXED_HALF;
    int64_t dot = (((int64_t) cx - x0) * gx + ((int64_t) cy - y0) * gy) >> 16;
    _twin_step_t row_t = _twin_step_init(dot * 65536, gy * 65536, len2);
    _twin_step_t px_t = _twin_step_init(0, gx * 65536, len2);
    for (twin_coord_t y = vis.top; y < vis.bottom; y++) {
        if (y == vis.top || gy) {
            _twin_step_t t = row_t;
            t.dq = px_t.dq;
            t.dr = px_t.dr;
            for (twin_coord_t i = 0; i < w; i++) {
                row[i] = _twin_lerp_q(c0, c1, t.q);
                _twin_step(&t);
            }
            _twin_step(&row_t);
        }
        twin_composite(dst, vis.left, y, &src, 0, 0, &msk, vis.left - mx,
                       y - my, TWIN_OVER, w, 1);
    }
    twin_pixmap_reset_xform_cache(&span);
    twin_free(row);
    _twin_mask_release(&m, sp);
}

void twin_paint_path(twin_pixmap_t *dst, twin_argb32_t argb, twin_path_t *path)
{
    twin_operand_t src = {.source_kind = TWIN_SOLID, .u.argb = argb};
    twin_scratch_t scratch;
    twin_scratch_t *sp = _twin_screen_scratch(dst, &scratch);
    twin_composite_path(dst, &src, 0, 0, path, TWIN_OVER, sp);
}

/*
 * True when nothing of @stroke drawn with @pen can reach the clip, so the
 * convolution can be skipped.  The extent is taken from the points directly
 * because twin_path_bounds() reports flat lines as empty.  Twice the pen's
 * reach covers square caps and miters.
 */
static bool _twin_stroke_misses_clip(twin_pixmap_t *dst,
                                     const twin_path_t *stroke,
                                     const twin_path_t *pen)
{
    if (!stroke->npoints)
        return true;
    int reach = 0;
    for (int i = 0; i < pen->npoints; i++) {
        int ax = abs(pen->points[i].x), ay = abs(pen->points[i].y);
        reach = max(reach, max(ax, ay));
    }
    twin_sfixed_t l = stroke->points[0].x, r = l;
    twin_sfixed_t t = stroke->points[0].y, b = t;
    for (int i = 1; i < stroke->npoints; i++) {
        l = min(l, stroke->points[i].x);
        r = max(r, stroke->points[i].x);
        t = min(t, stroke->points[i].y);
        b = max(b, stroke->points[i].y);
    }
    twin_coord_t grow = twin_sfixed_trunc(twin_sfixed_ceil(2 * reach)) + 2;
    twin_rect_t clip = twin_pixmap_get_clip(dst);
    return twin_sfixed_trunc(l) - grow >= clip.right ||
           twin_sfixed_trunc(twin_sfixed_ceil(r)) + grow <= clip.left ||
           twin_sfixed_trunc(t) - grow >= clip.bottom ||
           twin_sfixed_trunc(twin_sfixed_ceil(b)) + grow <= clip.top;
}

void twin_composite_stroke(twin_pixmap_t *dst,
                           twin_operand_t *src,
                           twin_coord_t src_x,
                           twin_coord_t src_y,
                           twin_path_t *stroke,
                           twin_fixed_t pen_width,
                           twin_operator_t operator,
                           twin_scratch_t * scratch)
{
    twin_screen_t *screen = dst->screen;
    twin_path_t *pen = _twin_path_acquire(screen);
    twin_path_t *path = _twin_path_acquire(screen);
    if (!pen || !path) {
        _twin_path_release(screen, path);
        _twin_path_release(screen, pen);
        return;
    }
    twin_matrix_t m = stroke->state.matrix;

    m.m[2][0] = 0;
    m.m[2][1] = 0;
    twin_path_set_matrix(pen, m);
    twin_path_set_cap_style(path, stroke->state.cap_style);
    twin_path_circle(pen, 0, 0, pen_width / 2);

    if (_twin_stroke_misses_clip(dst, stroke, pen)) {
        _twin_path_release(screen, path);
        _twin_path_release(screen, pen);
        return;
    }
    twin_path_convolve(path, stroke, pen, scratch);
    twin_composite_path(dst, src, src_x, src_y, path, operator, scratch);
    _twin_path_release(screen, path);
    _twin_path_release(screen, pen);
}

void twin_paint_stroke(twin_pixmap_t *dst,
                       twin_argb32_t argb,
                       twin_path_t *stroke,
                       twin_fixed_t pen_width)
{
    twin_operand_t src = {.source_kind = TWIN_SOLID, .u.argb = argb};
    twin_scratch_t scratch;
    twin_scratch_t *sp = _twin_screen_scratch(dst, &scratch);
    twin_composite_stroke(dst, &src, 0, 0, stroke, pen_width, TWIN_OVER, sp);
}
