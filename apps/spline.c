/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>

#include <twin.h>

#include "apps_spline.h"

#define D(x) twin_double_to_fixed(x)
#define CONTROL_POINT_RADIUS 10
#define BACKBONE_WIDTH 2
#define AUX_LINE_WIDTH 2

static inline twin_pixmap_t *_apps_spline_pixmap(twin_custom_widget_t *spline)
{
    return twin_custom_widget_pixmap(spline);
}

typedef struct {
    int n_points;
    twin_point_t *points;
    int which;
    twin_fixed_t line_width;
    twin_cap_t cap_style;
    twin_matrix_t transition;
    twin_matrix_t inverse_transition;
} apps_spline_data_t;

static void _init_control_point(apps_spline_data_t *spline)
{
    const int init_point_quad[3][2] = {
        {100, 100},
        {200, 100},
        {300, 100},
    };
    const int init_point_cubic[4][2] = {
        {100, 100},
        {280, 280},
        {100, 280},
        {280, 100},
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
                           twin_custom_widget_t *custom,
                           apps_spline_data_t *spline,
                           int idx1,
                           int idx2)
{
    twin_path_move(path, spline->points[idx1].x, spline->points[idx1].y);
    twin_path_draw(path, spline->points[idx2].x, spline->points[idx2].y);
    twin_paint_stroke(_apps_spline_pixmap(custom), 0xc08000c0, path,
                      twin_int_to_fixed(AUX_LINE_WIDTH));
    twin_path_empty(path);
}

static void _apps_spline_paint(twin_custom_widget_t *custom)
{
    apps_spline_data_t *spline =
        (apps_spline_data_t *) twin_custom_widget_data(custom);
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
    twin_paint_stroke(_apps_spline_pixmap(custom), 0xff404040, path,
                      spline->line_width);
    twin_path_set_cap_style(path, TwinCapButt);
    twin_paint_stroke(_apps_spline_pixmap(custom), 0xffffff00, path,
                      twin_int_to_fixed(BACKBONE_WIDTH));
    twin_path_empty(path);
    if (spline->n_points == 4) {
        _draw_aux_line(path, custom, spline, 0, 1);
        _draw_aux_line(path, custom, spline, 3, 2);
    } else if (spline->n_points == 3) {
        _draw_aux_line(path, custom, spline, 0, 1);
        _draw_aux_line(path, custom, spline, 1, 2);
    }

    for (int i = 0; i < spline->n_points; i++) {
        twin_path_empty(path);
        twin_path_circle(path, spline->points[i].x, spline->points[i].y,
                         twin_int_to_fixed(CONTROL_POINT_RADIUS));
        twin_paint_path(_apps_spline_pixmap(custom), 0x40004020, path);
    }
    twin_path_destroy(path);
}

static void _apps_spline_button_signal(twin_button_t *button,
                                       twin_button_signal_t signal,
                                       void *closure)
{
    (void) button; /* unused parameter */
    if (signal != TwinButtonSignalDown)
        return;

    twin_custom_widget_t *custom = closure;
    apps_spline_data_t *spline =
        (apps_spline_data_t *) twin_custom_widget_data(custom);
    spline->n_points = (spline->n_points == 3) ? 4 : 3;
    _init_control_point(spline);
    twin_custom_widget_queue_paint(custom);
}

static twin_dispatch_result_t _apps_spline_update_pos(
    twin_custom_widget_t *custom,
    twin_event_t *event)
{
    apps_spline_data_t *spline =
        (apps_spline_data_t *) twin_custom_widget_data(custom);
    if (spline->which < 0)
        return TwinDispatchContinue;
    twin_fixed_t x = twin_int_to_fixed(event->u.pointer.x);
    twin_fixed_t y = twin_int_to_fixed(event->u.pointer.y);

    spline->points[spline->which].x =
        twin_matrix_transform_x(&(spline->inverse_transition), x, y);
    spline->points[spline->which].y =
        twin_matrix_transform_y(&(spline->inverse_transition), x, y);
    twin_custom_widget_queue_paint(custom);
    /* Parent will be repainted automatically */
    return TwinDispatchDone;
}

static int _apps_spline_hit(apps_spline_data_t *spline,
                            twin_fixed_t x,
                            twin_fixed_t y)
{
    int i;
    for (i = 0; i < spline->n_points; i++) {
        twin_fixed_t px = twin_matrix_transform_x(
            &(spline->transition), spline->points[i].x, spline->points[i].y);
        twin_fixed_t py = twin_matrix_transform_y(
            &(spline->transition), spline->points[i].x, spline->points[i].y);
        if (twin_fixed_abs(x - px) < twin_int_to_fixed(CONTROL_POINT_RADIUS) &&
            twin_fixed_abs(y - py) < twin_int_to_fixed(CONTROL_POINT_RADIUS))
            return i;
    }
    return -1;
}

static twin_dispatch_result_t _apps_spline_dispatch(twin_widget_t *widget,
                                                    twin_event_t *event)
{
    twin_custom_widget_t *custom = twin_widget_get_custom(widget);
    if (!custom)
        return TwinDispatchContinue;

    apps_spline_data_t *spline =
        (apps_spline_data_t *) twin_custom_widget_data(custom);

    switch (event->kind) {
    case TwinEventPaint:
        _apps_spline_paint(custom);
        break;
    case TwinEventButtonDown:
        spline->which =
            _apps_spline_hit(spline, twin_int_to_fixed(event->u.pointer.x),
                             twin_int_to_fixed(event->u.pointer.y));
        return _apps_spline_update_pos(custom, event);
        break;
    case TwinEventMotion:
        return _apps_spline_update_pos(custom, event);
        break;
    case TwinEventButtonUp:
        if (spline->which < 0)
            return TwinDispatchContinue;
        _apps_spline_update_pos(custom, event);
        spline->which = -1;
        return TwinDispatchDone;
        break;
    default:
        break;
    }
    return TwinDispatchContinue;
}

static twin_custom_widget_t *_apps_spline_init(twin_box_t *parent, int n_points)
{
    twin_custom_widget_t *custom = twin_custom_widget_create(
        parent, 0xffffffff, 0, parent->widget.window->screen->height * 2 / 3, 1,
        1, _apps_spline_dispatch, sizeof(apps_spline_data_t));
    if (!custom)
        return NULL;

    apps_spline_data_t *spline =
        (apps_spline_data_t *) twin_custom_widget_data(custom);
    spline->line_width = twin_int_to_fixed(100);
    spline->cap_style = TwinCapRound;
    twin_matrix_identity(&spline->transition);
    twin_matrix_rotate(&spline->transition, TWIN_ANGLE_11_25);
    twin_matrix_identity(&spline->inverse_transition);
    twin_matrix_rotate(&spline->inverse_transition, -TWIN_ANGLE_11_25);
    spline->points = calloc(n_points, sizeof(twin_point_t));
    spline->n_points = n_points;
    spline->which = -1;
    _init_control_point(spline);

    twin_button_t *button =
        twin_button_create(parent, "Switch curve", 0xffae0000, D(10),
                           TwinStyleBold | TwinStyleOblique);
    twin_widget_set(&button->label.widget, 0xc0808080);
    button->signal = _apps_spline_button_signal;
    button->closure = custom;
    button->label.widget.shape = TwinShapeRectangle;

    return custom;
}

static twin_custom_widget_t *apps_spline_create(twin_box_t *parent,
                                                int n_points)
{
    return _apps_spline_init(parent, n_points);
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
    twin_custom_widget_t *spline = apps_spline_create(&toplevel->box, 4);
    (void) spline;
    twin_toplevel_show(toplevel);
}
