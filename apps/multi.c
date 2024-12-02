/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "apps_multi.h"

#define D(x) twin_double_to_fixed(x)
#define ASSET_PATH "assets/"

static void apps_line_start(twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_window_t *window = twin_window_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, false);
    twin_pixmap_t *pixmap = window->pixmap;
    twin_path_t *stroke = twin_path_create();
    twin_fixed_t fy;

    twin_path_translate(stroke, D(200), D(200));
    twin_fill(pixmap, 0xffffffff, TWIN_SOURCE, 0, 0, w, h);

    twin_window_set_name(window, "line");

    for (fy = 0; fy < 150; fy += 40) {
        twin_path_move(stroke, D(-150), -D(fy));
        twin_path_draw(stroke, D(150), D(fy));
    }
    twin_path_set_cap_style(stroke, TwinCapProjecting);
    twin_paint_stroke(pixmap, 0xff000000, stroke, D(10));
    twin_path_destroy(stroke);
    twin_window_show(window);
}

static void apps_circletext_start(twin_screen_t *screen,
                                  int x,
                                  int y,
                                  int w,
                                  int h)
{
    twin_window_t *window = twin_window_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, false);
    int wid = window->client.right - window->client.left;
    int hei = window->client.bottom - window->client.top;
    twin_pixmap_t *pixmap = window->pixmap;
    twin_path_t *path = twin_path_create();
    twin_path_t *pen = twin_path_create();
    twin_pixmap_t *alpha = twin_pixmap_create(TWIN_A8, wid, hei);
    int s;
    twin_operand_t source, mask;

    twin_fill(pixmap, 0xffffffff, TWIN_SOURCE, 0, 0, wid, hei);
    twin_window_set_name(window, "circletext");

    twin_path_set_font_style(path, TwinStyleUnhinted);
    twin_path_circle(pen, 0, 0, D(1));

    twin_path_translate(path, D(200), D(200));
    twin_path_set_font_size(path, D(15));
    for (s = 0; s < 41; s++) {
        twin_state_t state = twin_path_save(path);
        twin_path_rotate(path, twin_degrees_to_angle(9 * s));
        twin_path_move(path, D(100), D(0));
        twin_path_utf8(path, "Hello, world!");
        twin_path_restore(path, &state);
    }
    twin_fill_path(alpha, path, 0, 0);
    twin_path_destroy(path);
    twin_path_destroy(pen);
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff000000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite(pixmap, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER, wid,
                   hei);
    twin_pixmap_destroy(alpha);
    twin_window_show(window);
}

static void apps_quickbrown_start(twin_screen_t *screen,
                                  int x,
                                  int y,
                                  int w,
                                  int h)
{
    twin_window_t *window = twin_window_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, false);
    int wid = window->client.right - window->client.left;
    int hei = window->client.bottom - window->client.top;
    twin_pixmap_t *pixmap = window->pixmap;
    twin_path_t *path = twin_path_create();
    twin_path_t *pen = twin_path_create();
    twin_pixmap_t *alpha = twin_pixmap_create(TWIN_A8, wid, hei);
    twin_operand_t source, mask;
    twin_fixed_t fx, fy;
    int s;

    twin_window_set_name(window, "Quick Brown");

    twin_fill(pixmap, 0xffffffff, TWIN_SOURCE, 0, 0, wid, hei);

    twin_path_circle(pen, 0, 0, D(1));

    fx = D(3);
    fy = D(8);
    for (s = 6; s < 36; s++) {
        twin_path_move(path, fx, fy);
        twin_path_set_font_size(path, D(s));
        twin_path_utf8(path, "the quick brown fox jumps over the lazy dog.");
        twin_path_utf8(path, "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG.");
        fy += D(s);
    }

    twin_fill_path(alpha, path, 0, 0);
    twin_path_destroy(path);
    twin_path_destroy(pen);
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff000000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite(pixmap, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER, wid,
                   hei);
    twin_pixmap_destroy(alpha);
    twin_window_show(window);
}

static void apps_ascii_start(twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_window_t *window = twin_window_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, false);
    int wid = window->client.right - window->client.left;
    int hei = window->client.bottom - window->client.top;
    twin_pixmap_t *pixmap = window->pixmap;
    twin_path_t *path = twin_path_create();
    twin_path_t *pen = twin_path_create();
    twin_pixmap_t *alpha = twin_pixmap_create(TWIN_A8, wid, hei);
    twin_operand_t source, mask;
    twin_fixed_t fx, fy;
    int s;

    twin_window_set_name(window, "ASCII");

    twin_fill(pixmap, 0xffffffff, TWIN_SOURCE, 0, 0, wid, hei);
    twin_path_circle(pen, 0, 0, D(1));

    fx = D(3);
    fy = D(8);
    for (s = 6; s < 36; s += 6) {
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

    twin_fill_path(alpha, path, 0, 0);
    twin_path_destroy(path);
    twin_path_destroy(pen);
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff000000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite(pixmap, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER, wid,
                   hei);
    twin_pixmap_destroy(alpha);
    twin_window_show(window);
}

static void apps_jelly_start(twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_window_t *window = twin_window_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, false);
    int wid = window->client.right - window->client.left;
    int hei = window->client.bottom - window->client.top;
    twin_pixmap_t *pixmap = window->pixmap;
    twin_path_t *path = twin_path_create();
    twin_fixed_t fx, fy;
    int s;

    twin_window_set_name(window, "Jelly");

    twin_fill(pixmap, 0xffffffff, TWIN_SOURCE, 0, 0, wid, hei);

    fx = D(3);
    fy = D(8);
    for (s = 6; s < 36; s += 2) {
        twin_path_set_font_size(path, D(s));
        fy += D(s + 2);
        twin_path_move(path, fx, fy);
#define TEXT "jelly text"
        /* twin_path_set_font_style (path, TwinStyleUnhinted); */
        twin_path_utf8(path, TEXT);
        twin_paint_path(pixmap, 0xff000000, path);
        twin_path_empty(path);
        {
            twin_text_metrics_t m;
            twin_path_t *stroke = twin_path_create();
            twin_path_set_matrix(stroke, twin_path_current_matrix(path));
            twin_text_metrics_utf8(path, TEXT, &m);
            twin_path_translate(stroke, TWIN_FIXED_HALF, TWIN_FIXED_HALF);
            twin_path_move(stroke, fx, fy);
            twin_path_draw(stroke, fx + m.width, fy);
            twin_paint_stroke(pixmap, 0xffff0000, stroke, D(1));
            twin_path_empty(stroke);
            twin_path_move(stroke, fx + m.left_side_bearing, fy - m.ascent);
            twin_path_draw(stroke, fx + m.right_side_bearing, fy - m.ascent);
            twin_path_draw(stroke, fx + m.right_side_bearing, fy + m.descent);
            twin_path_draw(stroke, fx + m.left_side_bearing, fy + m.descent);
            twin_path_draw(stroke, fx + m.left_side_bearing, fy - m.ascent);
            twin_paint_stroke(pixmap, 0xff00ff00, stroke, D(1));
            twin_path_destroy(stroke);
        }
    }
    twin_path_destroy(path);
    twin_window_show(window);
}

static void apps_test(twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_pixmap_t *raw_background =
        twin_pixmap_from_file(ASSET_PATH "tux.png", TWIN_ARGB32);
    twin_window_t *window = twin_window_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, true);
    twin_window_set_name(window, "Test");
    twin_pixmap_t *scaled_background = twin_pixmap_create(
        TWIN_ARGB32, window->pixmap->width, window->pixmap->height);
    twin_fixed_t sx, sy;
    sx = twin_fixed_div(
        twin_int_to_fixed(raw_background->width),
        twin_int_to_fixed(window->client.right - window->client.left));
    sy = twin_fixed_div(
        twin_int_to_fixed(raw_background->height),
        twin_int_to_fixed(window->client.bottom - window->client.top));

    twin_matrix_scale(&raw_background->transform, sx, sy);
    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = raw_background,
    };

    twin_composite(scaled_background, 0, 0, &srcop, 0, 0, 0, 0, 0, TWIN_SOURCE,
                   screen->width, screen->height);

    twin_pointer_t src, dst;
    for (int y = window->client.top; y < window->client.bottom; y++)
        for (int x = window->client.left; x < window->client.right; x++) {
            src =
                twin_pixmap_pointer(scaled_background, x - window->client.left,
                                    y - window->client.top);
            dst = twin_pixmap_pointer(window->pixmap, x, y);
            *dst.argb32 = *src.argb32 | 0xff000000;
        }

    twin_stack_blur(window->pixmap, 5, window->client.left,
                    window->client.right, window->client.top,
                    window->client.bottom);
    twin_window_show(window);
    twin_pixmap_destroy(scaled_background);
}

void apps_multi_start(twin_screen_t *screen,
                      const char *name,
                      int x,
                      int y,
                      int w,
                      int h)
{
    (void) name;
    apps_circletext_start(screen, x, y, w, h);
    apps_line_start(screen, x += 20, y += 20, w, h);
    apps_quickbrown_start(screen, x += 20, y += 20, w, h);
    apps_ascii_start(screen, x += 20, y += 20, w, h);
    apps_jelly_start(screen, x += 20, y += 20, w / 2, h);
    apps_test(screen, x, y, w, h);
}
