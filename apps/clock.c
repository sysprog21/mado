/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <twin.h>

#include "apps_clock.h"

#define D(x) twin_double_to_fixed(x)

#define APPS_CLOCK_BACKGROUND 0xff3b80ae
#define APPS_CLOCK_HOUR 0x80808080
#define APPS_CLOCK_HOUR_OUT 0x30000000
#define APPS_CLOCK_MINUTE 0x80808080
#define APPS_CLOCK_MINUTE_OUT 0x30000000
#define APPS_CLOCK_SECOND 0x80808080
#define APPS_CLOCK_SECOND_OUT 0x30000000
#define APPS_CLOCK_TIC 0xffbababa
#define APPS_CLOCK_NUMBERS 0xffdedede
#define APPS_CLOCK_WATER 0x60200000
#define APPS_CLOCK_WATER_OUT 0x40404040
#define APPS_CLOCK_WATER_UNDER 0x60400000
#define APPS_CLOCK_BORDER 0xffbababa
#define APPS_CLOCK_BORDER_WIDTH D(0.01)

static inline twin_pixmap_t *_apps_clock_pixmap(twin_custom_widget_t *clock)
{
    return twin_custom_widget_pixmap(clock);
}

typedef struct {
    twin_timeout_t *timeout;
} apps_clock_data_t;

static void apps_clock_set_transform(twin_custom_widget_t *clock,
                                     twin_path_t *path)
{
    twin_fixed_t scale;

    scale = (TWIN_FIXED_ONE - APPS_CLOCK_BORDER_WIDTH * 3) / 2;
    twin_path_scale(path, twin_custom_widget_width(clock) * scale,
                    twin_custom_widget_height(clock) * scale);

    twin_path_translate(path, TWIN_FIXED_ONE + APPS_CLOCK_BORDER_WIDTH * 3,
                        TWIN_FIXED_ONE + APPS_CLOCK_BORDER_WIDTH * 3);

    twin_path_rotate(path, -TWIN_ANGLE_90);
}

static void apps_clock_hand(twin_custom_widget_t *clock,
                            twin_angle_t angle,
                            twin_fixed_t len,
                            twin_fixed_t fill_width,
                            twin_fixed_t out_width,
                            twin_argb32_t fill_pixel,
                            twin_argb32_t out_pixel)
{
    twin_path_t *stroke = twin_path_create();
    twin_path_t *pen = twin_path_create();
    twin_path_t *path = twin_path_create();
    twin_matrix_t m;

    apps_clock_set_transform(clock, stroke);

    twin_path_rotate(stroke, angle);
    twin_path_move(stroke, D(0), D(0));
    twin_path_draw(stroke, len, D(0));

    m = twin_path_current_matrix(stroke);
    m.m[2][0] = 0;
    m.m[2][1] = 0;
    twin_path_set_matrix(pen, m);
    twin_path_set_matrix(path, m);
    twin_path_circle(pen, 0, 0, fill_width);
    twin_path_convolve(path, stroke, pen);

    twin_paint_path(_apps_clock_pixmap(clock), fill_pixel, path);

    twin_paint_stroke(_apps_clock_pixmap(clock), out_pixel, path, out_width);

    twin_path_destroy(path);
    twin_path_destroy(pen);
    twin_path_destroy(stroke);
}

static void _apps_clock_date(twin_custom_widget_t *clock, struct tm *t)
{
    char text[7];
    twin_text_metrics_t metrics;
    twin_fixed_t height, width;

    strftime(text, sizeof(text), "%b %d", t);

    twin_path_t *path = twin_path_create();
    if (!path)
        return;

    apps_clock_set_transform(clock, path);
    twin_path_rotate(path, TWIN_ANGLE_90);
    twin_path_translate(path, D(0.8), 0);
    twin_path_set_font_size(path, D(0.25));
    twin_path_set_font_style(path, TwinStyleUnhinted);
    twin_text_metrics_utf8(path, text, &metrics);
    height = metrics.ascent + metrics.descent;
    width = metrics.right_side_bearing - metrics.left_side_bearing;
    twin_path_move(path, -width, metrics.ascent - height / 2);
    twin_path_utf8(path, text);
    twin_paint_path(_apps_clock_pixmap(clock), APPS_CLOCK_WATER, path);
    twin_path_destroy(path);
}

static twin_angle_t apps_clock_minute_angle(int min)
{
    return min * TWIN_ANGLE_360 / 60;
}

static void _apps_clock_face(twin_custom_widget_t *clock)
{
    twin_path_t *path = twin_path_create();
    int m;

    apps_clock_set_transform(clock, path);

    twin_path_circle(path, 0, 0, TWIN_FIXED_ONE);

    twin_paint_path(_apps_clock_pixmap(clock), APPS_CLOCK_BACKGROUND, path);

    twin_paint_stroke(_apps_clock_pixmap(clock), APPS_CLOCK_BORDER, path,
                      APPS_CLOCK_BORDER_WIDTH);

    twin_path_set_font_size(path, D(0.2));
    twin_path_set_font_style(path, TwinStyleUnhinted);

    for (m = 1; m <= 60; m++) {
        twin_state_t state = twin_path_save(path);
        twin_path_rotate(path, apps_clock_minute_angle(m) + TWIN_ANGLE_90);
        twin_path_empty(path);
        if (m % 5 != 0) {
            twin_path_move(path, 0, -TWIN_FIXED_ONE);
            twin_path_draw(path, 0, -D(0.9));
            twin_paint_stroke(_apps_clock_pixmap(clock), APPS_CLOCK_TIC, path,
                              D(0.01));
        } else {
            char hour[3];
            twin_text_metrics_t metrics;
            twin_fixed_t width;
            twin_fixed_t left;

            sprintf(hour, "%d", m / 5);
            twin_text_metrics_utf8(path, hour, &metrics);
            width = metrics.right_side_bearing - metrics.left_side_bearing;
            left = -width / 2 - metrics.left_side_bearing;
            twin_path_move(path, left, -D(0.98) + metrics.ascent);
            twin_path_utf8(path, hour);
            twin_paint_path(_apps_clock_pixmap(clock), APPS_CLOCK_NUMBERS,
                            path);
        }
        twin_path_restore(path, &state);
    }

    twin_path_destroy(path);
}

static twin_time_t _apps_clock_interval(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return 1000 - (tv.tv_usec / 1000);
}

static void _apps_clock_paint(twin_custom_widget_t *clock)
{
    struct timeval tv;
    twin_angle_t second_angle, minute_angle, hour_angle;
    struct tm t;

    gettimeofday(&tv, NULL);

    localtime_r(&tv.tv_sec, &t);

    _apps_clock_face(clock);

    _apps_clock_date(clock, &t);

    second_angle =
        ((t.tm_sec * 100 + tv.tv_usec / 10000) * TWIN_ANGLE_360) / 6000;
    minute_angle = apps_clock_minute_angle(t.tm_min) + second_angle / 60;
    hour_angle = (t.tm_hour * TWIN_ANGLE_360 + minute_angle) / 12;
    apps_clock_hand(clock, hour_angle, D(0.4), D(0.07), D(0.01),
                    APPS_CLOCK_HOUR, APPS_CLOCK_HOUR_OUT);
    apps_clock_hand(clock, minute_angle, D(0.8), D(0.05), D(0.01),
                    APPS_CLOCK_MINUTE, APPS_CLOCK_MINUTE_OUT);
    apps_clock_hand(clock, second_angle, D(0.9), D(0.01), D(0.01),
                    APPS_CLOCK_SECOND, APPS_CLOCK_SECOND_OUT);
}

static twin_time_t _apps_clock_timeout(twin_time_t now, void *closure)
{
    (void) now; /* unused parameter */
    twin_custom_widget_t *clock = closure;
    twin_custom_widget_queue_paint(clock);
    return _apps_clock_interval();
}

static twin_dispatch_result_t _apps_clock_dispatch(twin_widget_t *widget,
                                                   twin_event_t *event)
{
    twin_custom_widget_t *custom = twin_widget_get_custom(widget);
    if (!custom)
        return TwinDispatchContinue;

    switch (event->kind) {
    case TwinEventPaint:
        _apps_clock_paint(custom);
        break;
    default:
        break;
    }
    return TwinDispatchContinue;
}

static twin_custom_widget_t *_apps_clock_init(twin_box_t *parent)
{
    twin_custom_widget_t *custom = twin_custom_widget_create(
        parent, 0, 0, 0, 1, 1, _apps_clock_dispatch, sizeof(apps_clock_data_t));
    if (!custom)
        return NULL;

    apps_clock_data_t *data =
        (apps_clock_data_t *) twin_custom_widget_data(custom);
    data->timeout =
        twin_set_timeout(_apps_clock_timeout, _apps_clock_interval(), custom);

    return custom;
}

static twin_custom_widget_t *apps_clock_create(twin_box_t *parent)
{
    return _apps_clock_init(parent);
}

void apps_clock_start(twin_screen_t *screen,
                      const char *name,
                      int x,
                      int y,
                      int w,
                      int h)
{
    twin_toplevel_t *toplevel = twin_toplevel_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, name);
    twin_custom_widget_t *clock = apps_clock_create(&toplevel->box);
    (void) clock;
    twin_toplevel_show(toplevel);
}
