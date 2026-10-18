/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <stddef.h>
#include <stdlib.h>
#include "apps_multi.h"

#define D(x) twin_double_to_fixed(x)
#define ASSET_PATH "assets/"

typedef struct {
    twin_point_t p1, p2;
} line_segment_t;

#define MAX_LINES 3

typedef struct {
    line_segment_t lines[MAX_LINES];
    int num_lines;
    int active_line;
    int active_point;
    twin_fixed_t line_width;
} multiline_data_t;

static inline twin_pixmap_t *_multiline_pixmap(twin_custom_widget_t *widget)
{
    return twin_custom_widget_pixmap(widget);
}

static void _multiline_paint(twin_custom_widget_t *custom)
{
    multiline_data_t *data =
        (multiline_data_t *) twin_custom_widget_data(custom);
    twin_pixmap_t *pixmap = _multiline_pixmap(custom);
    int i;

    for (i = 0; i < data->num_lines; i++) {
        twin_path_t *path = twin_path_create();
        twin_path_set_cap_style(path, TwinCapRound);
        twin_path_move(path, data->lines[i].p1.x, data->lines[i].p1.y);
        twin_path_draw(path, data->lines[i].p2.x, data->lines[i].p2.y);

        twin_argb32_t color =
            (i == data->active_line) ? 0xffff0000 : 0xff000000;
        twin_paint_stroke(pixmap, color, path, data->line_width);

        /* Draw center line indicator */
        twin_path_set_cap_style(path, TwinCapButt);
        twin_paint_stroke(pixmap, 0xffff0000, path, twin_int_to_fixed(2));
        twin_path_destroy(path);

        /* Draw control points */
        twin_path_t *pt = twin_path_create();
        twin_path_circle(pt, data->lines[i].p1.x, data->lines[i].p1.y, D(5));
        twin_paint_path(pixmap, 0xff0000ff, pt);
        twin_path_empty(pt);
        twin_path_circle(pt, data->lines[i].p2.x, data->lines[i].p2.y, D(5));
        twin_paint_path(pixmap, 0xff00ff00, pt);
        twin_path_destroy(pt);
    }
}

static int _multiline_hit_test(multiline_data_t *data,
                               twin_fixed_t x,
                               twin_fixed_t y,
                               int *point_idx)
{
    int i;
    twin_fixed_t threshold = D(10);

    for (i = 0; i < data->num_lines; i++) {
        if (twin_fixed_abs(x - data->lines[i].p1.x) < threshold &&
            twin_fixed_abs(y - data->lines[i].p1.y) < threshold) {
            *point_idx = 0;
            return i;
        }
        if (twin_fixed_abs(x - data->lines[i].p2.x) < threshold &&
            twin_fixed_abs(y - data->lines[i].p2.y) < threshold) {
            *point_idx = 1;
            return i;
        }
    }
    return -1;
}

static twin_dispatch_result_t _multiline_dispatch(twin_widget_t *widget,
                                                  twin_event_t *event,
                                                  void *closure)
{
    (void) closure;

    twin_custom_widget_t *custom = twin_widget_get_custom(widget);
    if (!custom)
        return TwinDispatchContinue;

    multiline_data_t *data =
        (multiline_data_t *) twin_custom_widget_data(custom);

    switch (event->kind) {
    case TwinEventPaint:
        _multiline_paint(custom);
        break;
    case TwinEventButtonDown: {
        twin_fixed_t x = twin_int_to_fixed(event->u.pointer.x);
        twin_fixed_t y = twin_int_to_fixed(event->u.pointer.y);
        int point_idx = 0;
        data->active_line = _multiline_hit_test(data, x, y, &point_idx);
        data->active_point = point_idx;
        twin_custom_widget_queue_paint(custom);
        return TwinDispatchDone;
    }
    case TwinEventMotion:
        if (data->active_line >= 0) {
            twin_fixed_t x = twin_int_to_fixed(event->u.pointer.x);
            twin_fixed_t y = twin_int_to_fixed(event->u.pointer.y);
            if (data->active_point == 0) {
                data->lines[data->active_line].p1.x = x;
                data->lines[data->active_line].p1.y = y;
            } else {
                data->lines[data->active_line].p2.x = x;
                data->lines[data->active_line].p2.y = y;
            }
            twin_custom_widget_queue_paint(custom);
            return TwinDispatchDone;
        }
        break;
    case TwinEventButtonUp:
        data->active_line = -1;
        twin_custom_widget_queue_paint(custom);
        return TwinDispatchDone;
    default:
        break;
    }
    return TwinDispatchContinue;
}

static bool apps_multiline_start(twin_screen_t *screen,
                                 int x,
                                 int y,
                                 int w,
                                 int h)
{
    twin_toplevel_t *toplevel = twin_toplevel_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, "Lines");
    if (!toplevel)
        return false;

    twin_custom_widget_t *custom = twin_custom_widget_create(
        twin_toplevel_box(toplevel), 0xffffffff, 0, 0, 1, 1,
        _multiline_dispatch, sizeof(multiline_data_t));
    if (!custom) {
        twin_toplevel_destroy(toplevel);
        return false;
    }

    multiline_data_t *data =
        (multiline_data_t *) twin_custom_widget_data(custom);
    data->num_lines = 3;
    data->active_line = -1;
    data->line_width = D(30);

    /* Initialize 3 lines */
    twin_fixed_t fy;
    int i = 0;
    for (fy = 0; i < 3; fy += 50, i++) {
        data->lines[i].p1.x = D(30);
        data->lines[i].p1.y = D(100 - fy);
        data->lines[i].p2.x = D(220);
        data->lines[i].p2.y = D(100 + fy);
    }

    twin_toplevel_show(toplevel);
    return true;
}

/*
 * The windows below paint from draw callbacks at whatever size the window
 * manager gives them: a desktop keeps the requested size, a phone or the
 * 'none' policy hands out the whole work area, and the mobile policy may
 * free a covered window's pixels and ask for them again.
 */

/* Client size of the window being drawn */
static void client_size(twin_window_t *window, int *w, int *h)
{
    twin_rect_t c = twin_window_client(window);
    *w = c.right - c.left;
    *h = c.bottom - c.top;
}

/* Scale for art designed for a 400x400 window, so it fits a smaller one */
static twin_fixed_t fit_400(int w, int h)
{
    int side = w < h ? w : h;
    return side >= 400 ? TWIN_FIXED_ONE
                       : twin_fixed_div(twin_int_to_fixed(side), D(400));
}

/* Open a window painted by @draw; @data and @destroy belong to it.  A new
 * window is damaged all over, so the first draw paints everything. */
static bool start_window(twin_screen_t *screen,
                         int x,
                         int y,
                         int w,
                         int h,
                         const char *name,
                         twin_draw_func_t draw,
                         void *data,
                         twin_destroy_func_t destroy)
{
    twin_window_t *window = twin_window_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h);
    if (!window)
        return false;
    twin_window_set_name(window, name);
    twin_window_set_client_data(window, data);
    twin_window_set_callbacks(window, draw, NULL, destroy);
    twin_window_draw(window);
    twin_window_show(window);
    return true;
}

/* Paint @path in black over white */
static void paint_text(twin_pixmap_t *pixmap, twin_path_t *path, int w, int h)
{
    twin_fill(pixmap, 0xffffffff, TWIN_SOURCE, 0, 0, w, h);
    twin_paint_path(pixmap, 0xff000000, path);
}

static void circletext_draw(twin_window_t *window)
{
    int w, h;
    client_size(window, &w, &h);
    twin_path_t *path = twin_path_create();
    if (!path)
        return;
    twin_fixed_t scale = fit_400(w, h);
    twin_path_set_font_style(path, TwinStyleUnhinted);
    twin_path_translate(path, twin_int_to_fixed(w / 2),
                        twin_int_to_fixed(h / 2));
    twin_path_scale(path, scale, scale);
    twin_path_set_font_size(path, D(15));
    for (int s = 0; s < 41; s++) {
        twin_state_t state = twin_path_save(path);
        twin_path_rotate(path, twin_degrees_to_angle(9 * s));
        twin_path_move(path, D(100), D(0));
        twin_path_utf8(path, "Hello, world!");
        twin_path_restore(path, &state);
    }
    paint_text(twin_window_pixmap(window), path, w, h);
    twin_path_destroy(path);
}

static void smiley_draw(twin_window_t *window)
{
    int w, h;
    client_size(window, &w, &h);
    twin_pixmap_t *pixmap = twin_window_pixmap(window);

    /* Clear background to white */
    twin_fill(pixmap, 0xffffffff, TWIN_SOURCE, 0, 0, w, h);

    /* Calculate center and dimensions */
    twin_fixed_t center_x = twin_int_to_fixed(w / 2);
    twin_fixed_t center_y = twin_int_to_fixed(h / 2);
    twin_fixed_t face_radius = D(70);
    twin_fixed_t mouth_radius = D(50);
    twin_fixed_t eye_radius = D(10);
    twin_fixed_t eye_offset_x = D(25);
    twin_fixed_t eye_offset_y = D(20);

    /* Draw face - yellow circle with black outline */
    twin_path_t *face = twin_path_create();
    twin_path_circle(face, center_x, center_y, face_radius);
    twin_paint_path(pixmap, 0xffffff00, face);         /* Yellow fill */
    twin_paint_stroke(pixmap, 0xff000000, face, D(5)); /* Black outline */
    twin_path_destroy(face);

    /* Draw left eye */
    twin_path_t *left_eye = twin_path_create();
    twin_path_circle(left_eye, center_x - eye_offset_x, center_y - eye_offset_y,
                     eye_radius);
    twin_paint_path(pixmap, 0xff000000, left_eye); /* Black */
    twin_path_destroy(left_eye);

    /* Draw right eye */
    twin_path_t *right_eye = twin_path_create();
    twin_path_circle(right_eye, center_x + eye_offset_x,
                     center_y - eye_offset_y, eye_radius);
    twin_paint_path(pixmap, 0xff000000, right_eye); /* Black */
    twin_path_destroy(right_eye);

    /* Draw smile - bottom half arc */
    twin_path_t *smile = twin_path_create();
    twin_path_arc(smile, center_x, center_y, mouth_radius, mouth_radius,
                  TWIN_ANGLE_0, TWIN_ANGLE_180);
    twin_paint_stroke(pixmap, 0xff000000, smile, D(5)); /* Black line */
    twin_path_destroy(smile);
}

static void ascii_draw(twin_window_t *window)
{
    int w, h;
    client_size(window, &w, &h);
    twin_path_t *path = twin_path_create();
    if (!path)
        return;
    twin_fixed_t fx = D(3), fy = D(8);
    for (int s = 6; s < 36; s += 6) {
        twin_path_set_font_size(path, D(s));
        fy += D(s + 2);
        twin_path_move(path, fx, fy);
        twin_path_utf8(path, " !\"#$%&'()*+,-./0123456789:;<=>?");
        fy += D(s + 2);
        twin_path_move(path, fx, fy);
        twin_path_utf8(path, "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_");
        fy += D(s + 2);
        twin_path_move(path, fx, fy);
        twin_path_utf8(path, "`abcdefghijklmnopqrstuvwxyz{|}~");
        fy += D(s + 2);
    }
    paint_text(twin_window_pixmap(window), path, w, h);
    twin_path_destroy(path);
}

static void draw_flower(twin_path_t *path,
                        twin_fixed_t radius,
                        int number_of_petals)
{
    const twin_angle_t angle_shift = TWIN_ANGLE_360 / number_of_petals;
    const twin_angle_t angle_start = angle_shift / 2;
    twin_fixed_t s, c;
    twin_sincos(-angle_start, &s, &c);
    twin_fixed_t p_x = twin_fixed_mul(radius, c);
    twin_fixed_t p_y = twin_fixed_mul(radius, s);
    twin_path_move(path, p_x, p_y);

    for (twin_angle_t a = angle_start; a <= TWIN_ANGLE_360; a += angle_shift) {
        twin_sincos(a, &s, &c);
        twin_fixed_t c_x = twin_fixed_mul(radius, c);
        twin_fixed_t c_y = twin_fixed_mul(radius, s);
        twin_fixed_t rx = radius;
        twin_fixed_t ry = radius * 3;
        twin_path_arc_ellipse(path, 1, 1, rx, ry, p_x, p_y, c_x, c_y,
                              a - angle_start);
        p_x = c_x;
        p_y = c_y;
    }

    twin_path_close(path);
}

static void flower_draw(twin_window_t *window)
{
    int w, h;
    client_size(window, &w, &h);
    twin_pixmap_t *pixmap = twin_window_pixmap(window);
    twin_fixed_t scale = fit_400(w, h);
    twin_fixed_t cx = twin_int_to_fixed(w / 2), cy = twin_int_to_fixed(h / 2);
    twin_path_t *stroke = twin_path_create();
    twin_path_t *path = twin_path_create();
    twin_fill(pixmap, 0xffffffff, TWIN_SOURCE, 0, 0, w, h);
    if (stroke && path) {
        twin_path_translate(path, cx, cy);
        twin_path_scale(path, twin_fixed_mul(scale, D(10)),
                        twin_fixed_mul(scale, D(10)));
        twin_path_translate(stroke, cx, cy);
        twin_path_scale(stroke, scale, scale);
        twin_path_move(stroke, D(-200), D(0));
        twin_path_draw(stroke, D(200), D(0));
        twin_path_move(stroke, D(0), D(200));
        twin_path_draw(stroke, D(0), D(-200));
        twin_path_set_cap_style(stroke, TwinCapProjecting);
        twin_paint_stroke(pixmap, 0xffcc9999, stroke,
                          twin_fixed_mul(scale, D(10)));
        draw_flower(path, D(3), 5);
        twin_paint_path(pixmap, 0xffe2d2d2, path);
    }
    if (stroke)
        twin_path_destroy(stroke);
    if (path)
        twin_path_destroy(path);
}

/*
 * Separable box blur, three passes approximating a Gaussian.  Demo-only:
 * the window system itself never blurs (shadows are synthesized).
 */
static void box_blur_line(twin_argb32_t *p,
                          int n,
                          int step,
                          int r,
                          twin_argb32_t *tmp)
{
    for (int i = 0; i < n; i++)
        tmp[i] = p[i * step];
    for (int i = 0; i < n; i++) {
        uint32_t sum[4] = {0, 0, 0, 0}, count = 0;
        for (int k = i - r; k <= i + r; k++) {
            if (k < 0 || k >= n)
                continue;
            for (int c = 0; c < 4; c++)
                sum[c] += (tmp[k] >> (c * 8)) & 0xff;
            count++;
        }
        twin_argb32_t v = 0;
        for (int c = 0; c < 4; c++)
            v |= (twin_argb32_t) (sum[c] / count) << (c * 8);
        p[i * step] = v;
    }
}

static void box_blur(twin_pixmap_t *px, int r)
{
    int w = twin_pixmap_width(px), h = twin_pixmap_height(px);
    twin_coord_t stride_bytes;
    twin_argb32_t *pixels = twin_pixmap_pixels(px, &stride_bytes);
    int stride = stride_bytes / 4;
    if (!pixels)
        return;
    twin_argb32_t *tmp = malloc(sizeof(*tmp) * (w > h ? w : h));
    if (!tmp)
        return;
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < h; y++)
            box_blur_line(pixels + y * stride, w, 1, r / 3 + 1, tmp);
        for (int x = 0; x < w; x++)
            box_blur_line(pixels + x, h, stride, r / 3 + 1, tmp);
    }
    free(tmp);
}

/* The window keeps the source image and blurs it at the window's size */
static void blur_draw(twin_window_t *window)
{
    twin_pixmap_t *raw_background = twin_window_client_data(window);
    int w, h;
    client_size(window, &w, &h);
    twin_pixmap_t *scaled = twin_pixmap_create(TWIN_ARGB32, w, h);
    if (!scaled)
        return;
    twin_matrix_t m;
    twin_matrix_identity(&m);
    twin_matrix_scale(
        &m,
        twin_fixed_div(twin_int_to_fixed(twin_pixmap_width(raw_background)),
                       twin_int_to_fixed(w)),
        twin_fixed_div(twin_int_to_fixed(twin_pixmap_height(raw_background)),
                       twin_int_to_fixed(h)));
    twin_pixmap_set_transform(raw_background, &m);
    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = raw_background,
    };
    twin_composite(scaled, 0, 0, &srcop, 0, 0, NULL, 0, 0, TWIN_SOURCE, w, h);
    box_blur(scaled, 8);

    /* Copy the blurred background to the window */
    srcop.u.pixmap = scaled;
    twin_composite(twin_window_pixmap(window), 0, 0, &srcop, 0, 0, NULL, 0, 0,
                   TWIN_SOURCE, w, h);
    twin_pixmap_destroy(scaled);
}

static void blur_destroy(twin_window_t *window)
{
    twin_pixmap_destroy(twin_window_client_data(window));
}

static bool apps_blur(twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_pixmap_t *raw_background = NULL;
#if defined(CONFIG_LOADER_PNG)
    raw_background = twin_pixmap_from_file(ASSET_PATH "tux.png", TWIN_ARGB32);
#endif
    if (!raw_background)
        return false;
    if (start_window(screen, x, y, w, h, "Blur", blur_draw, raw_background,
                     blur_destroy))
        return true;
    twin_pixmap_destroy(raw_background);
    return false;
}

bool apps_multi_start(twin_screen_t *screen,
                      const char *name,
                      int x,
                      int y,
                      int w,
                      int h)
{
    (void) name;
    /* Started if any of its windows opened */
    bool any = start_window(screen, x, y, w, h, "circletext", circletext_draw,
                            NULL, NULL);
    any |= apps_multiline_start(screen, x += 20, y += 20, w * 2 / 3, h * 2 / 3);
    any |= start_window(screen, x += 20, y += 20, 200, 200, "Smiley",
                        smiley_draw, NULL, NULL);
    any |= start_window(screen, x += 20, y += 20, w, h, "ASCII", ascii_draw,
                        NULL, NULL);
    any |= start_window(screen, x += 20, y += 20, w, h, "Flower", flower_draw,
                        NULL, NULL);
    any |= apps_blur(screen, x += 20, y += 20, w / 2, h / 2);
    return any;
}
