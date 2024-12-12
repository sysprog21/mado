/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>

#include "twin_private.h"

#include "apps_spline.h"

#define D(x) twin_double_to_fixed(x)

#define _apps_spline_pixmap(spline) ((spline)->widget.window->pixmap)

typedef struct _apps_spline {
    twin_widget_t widget;
    int n_points;
    twin_point_t *points;
    int which;
    twin_fixed_t line_width;
    twin_cap_t cap_style;
    twin_matrix_t transition;
    twin_matrix_t inverse_transition;
} apps_spline_t;

static void _init_control_point(apps_spline_t *spline)
{
    const int init_point_quad[3][2] = {
        {100, 100},
        {200, 100},
        {300, 100},
    };
    const int init_point_cubic[4][2] = {
        {100, 100},
        {300, 300},
        {100, 300},
        {300, 100},
    };
    const int(*init_point)[2];
    if (spline->n_points == 4) {
        init_point = init_point_cubic;
    } else if (spline->n_points == 3) {
        init_point = init_point_quad;
    }
    for (int i = 0; i < spline->n_points; i++) {
        spline->points[i].x = twin_int_to_fixed(init_point[i][0]);
        spline->points[i].y = twin_int_to_fixed(init_point[i][1]);
    }
}

static void _draw_aux_line(twin_path_t *path,
                           apps_spline_t *spline,
                           int idx1,
                           int idx2)
{
    twin_path_move(path, spline->points[idx1].x, spline->points[idx1].y);
    twin_path_draw(path, spline->points[idx2].x, spline->points[idx2].y);
    twin_paint_stroke(_apps_spline_pixmap(spline), 0xc08000c0, path,
                      twin_int_to_fixed(2));
    twin_path_empty(path);
}

static void _apps_spline_paint(apps_spline_t *spline)
{
    twin_path_t *path;
    path = twin_path_create();
    twin_path_set_cap_style(path, spline->cap_style);
    twin_path_set_matrix(path, spline->transition);

    twin_path_move(path, spline->points[0].x, spline->points[0].y);
    if (spline->n_points == 4) {
        twin_path_curve(path, spline->points[1].x, spline->points[1].y,
                        spline->points[2].x, spline->points[2].y,
                        spline->points[3].x, spline->points[3].y);
    } else if (spline->n_points == 3) {
        twin_path_quadratic_curve(path, spline->points[1].x,
                                  spline->points[1].y, spline->points[2].x,
                                  spline->points[2].y);
    }
    twin_paint_stroke(_apps_spline_pixmap(spline), 0xff404040, path,
                      spline->line_width);
    twin_path_set_cap_style(path, TwinCapButt);
    twin_paint_stroke(_apps_spline_pixmap(spline), 0xffffff00, path,
                      twin_int_to_fixed(2));
    twin_path_empty(path);
    if (spline->n_points == 4) {
        _draw_aux_line(path, spline, 0, 1);
        _draw_aux_line(path, spline, 3, 2);
    } else if (spline->n_points == 3) {
        _draw_aux_line(path, spline, 0, 1);
        _draw_aux_line(path, spline, 1, 2);
    }

    for (int i = 0; i < spline->n_points; i++) {
        twin_path_empty(path);
        twin_path_circle(path, spline->points[i].x, spline->points[i].y,
                         twin_int_to_fixed(10));
        twin_paint_path(_apps_spline_pixmap(spline), 0x40004020, path);
    }
    twin_path_destroy(path);
}

static void _apps_spline_button_signal(maybe_unused twin_button_t *button,
                                       twin_button_signal_t signal,
                                       void *closure)
{
    if (signal != TwinButtonSignalDown)
        return;

    apps_spline_t *spline = closure;
    spline->n_points = (spline->n_points == 3) ? 4 : 3;
    _init_control_point(spline);
    _twin_widget_queue_paint(&spline->widget);
}

static twin_dispatch_result_t _apps_spline_update_pos(apps_spline_t *spline,
                                                      twin_event_t *event)
{
    if (spline->which < 0)
        return TwinDispatchContinue;
    twin_fixed_t x = twin_int_to_fixed(event->u.pointer.x);
    twin_fixed_t y = twin_int_to_fixed(event->u.pointer.y);

    spline->points[spline->which].x = twin_sfixed_to_fixed(
        _twin_matrix_x(&(spline->inverse_transition), x, y));
    spline->points[spline->which].y = twin_sfixed_to_fixed(
        _twin_matrix_y(&(spline->inverse_transition), x, y));
    _twin_widget_queue_paint(&spline->widget);
    return TwinDispatchDone;
}

static int _apps_spline_hit(apps_spline_t *spline,
                            twin_fixed_t x,
                            twin_fixed_t y)
{
    int i;
    for (i = 0; i < spline->n_points; i++) {
        twin_fixed_t px = twin_sfixed_to_fixed(_twin_matrix_x(
            &(spline->transition), spline->points[i].x, spline->points[i].y));
        twin_fixed_t py = twin_sfixed_to_fixed(_twin_matrix_y(
            &(spline->transition), spline->points[i].x, spline->points[i].y));
        if (twin_fixed_abs(x - px) < spline->line_width / 2 &&
            twin_fixed_abs(y - py) < spline->line_width / 2)
            return i;
    }
    return -1;
}

static twin_dispatch_result_t _apps_spline_dispatch(twin_widget_t *widget,
                                                    twin_event_t *event)
{
    apps_spline_t *spline = (apps_spline_t *) widget;

    if (_twin_widget_dispatch(widget, event) == TwinDispatchDone)
        return TwinDispatchDone;
    switch (event->kind) {
    case TwinEventPaint:
        _apps_spline_paint(spline);
        break;
    case TwinEventButtonDown:
        spline->which =
            _apps_spline_hit(spline, twin_int_to_fixed(event->u.pointer.x),
                             twin_int_to_fixed(event->u.pointer.y));
        return _apps_spline_update_pos(spline, event);
        break;
    case TwinEventMotion:
        return _apps_spline_update_pos(spline, event);
        break;
    case TwinEventButtonUp:
        if (spline->which < 0)
            return TwinDispatchContinue;
        _apps_spline_update_pos(spline, event);
        spline->which = -1;
        return TwinDispatchDone;
        break;
    default:
        break;
    }
    return TwinDispatchContinue;
}

static void _apps_spline_init(apps_spline_t *spline,
                              twin_box_t *parent,
                              twin_dispatch_proc_t dispatch,
                              int n_points)
{
    static twin_widget_layout_t preferred = {0, 0, 1, 1};
    preferred.height = parent->widget.window->screen->height * 2 / 3;
    _twin_widget_init(&spline->widget, parent, 0, preferred, dispatch);
    twin_widget_set(&spline->widget, 0xffffffff);
    spline->line_width = twin_int_to_fixed(100);
    spline->cap_style = TwinCapRound;
    twin_matrix_identity(&spline->transition);
    twin_matrix_rotate(&spline->transition, TWIN_ANGLE_11_25);
    twin_matrix_identity(&spline->inverse_transition);
    twin_matrix_rotate(&spline->inverse_transition, -TWIN_ANGLE_11_25);
    spline->points = calloc(n_points, sizeof(twin_point_t));
    spline->n_points = n_points;
    _init_control_point(spline);
    twin_button_t *button =
        twin_button_create(parent, "Switch curve", 0xffae0000, D(10),
                           TwinStyleBold | TwinStyleOblique);
    twin_widget_set(&button->label.widget, 0xc0808080);
    button->signal = _apps_spline_button_signal;
    button->closure = spline;
    button->label.widget.shape = TwinShapeRectangle;
}

static apps_spline_t *apps_spline_create(twin_box_t *parent, int n_points)
{
    apps_spline_t *spline = malloc(sizeof(apps_spline_t));

    _apps_spline_init(spline, parent, _apps_spline_dispatch, n_points);
    return spline;
}

void apps_spline_start(twin_screen_t *screen,
                       const char *name,
                       int x,
                       int y,
                       int w,
                       int h)
{
    twin_toplevel_t *toplevel = twin_toplevel_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, name);
    apps_spline_t *spline = apps_spline_create(&toplevel->box, 4);
    (void) spline;
    twin_toplevel_show(toplevel);
}
