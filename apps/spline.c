/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdio.h>
#include <twinint.h>

#include "apps_spline.h"

#define D(x) twin_double_to_fixed(x)

#define _apps_spline_pixmap(spline) ((spline)->widget.window->pixmap)

#define NPT 4

typedef struct _apps_spline {
    twin_widget_t widget;
    twin_point_t points[NPT];
    int which;
    twin_fixed_t line_width;
    twin_cap_t cap_style;
} apps_spline_t;

static void _apps_spline_paint(apps_spline_t *spline)
{
    twin_path_t *path;
    int i;

    path = twin_path_create();
    twin_path_set_cap_style(path, spline->cap_style);
    twin_path_move(path, spline->points[0].x, spline->points[0].y);
    twin_path_curve(path, spline->points[1].x, spline->points[1].y,
                    spline->points[2].x, spline->points[2].y,
                    spline->points[3].x, spline->points[3].y);
    twin_paint_stroke(_apps_spline_pixmap(spline), 0xff404040, path,
                      spline->line_width);
    twin_path_set_cap_style(path, TwinCapButt);
    twin_paint_stroke(_apps_spline_pixmap(spline), 0xffffff00, path,
                      twin_int_to_fixed(2));

    twin_path_empty(path);
    twin_path_move(path, spline->points[0].x, spline->points[0].y);
    twin_path_draw(path, spline->points[1].x, spline->points[1].y);
    twin_paint_stroke(_apps_spline_pixmap(spline), 0xc08000c0, path,
                      twin_int_to_fixed(2));
    twin_path_empty(path);
    twin_path_move(path, spline->points[3].x, spline->points[3].y);
    twin_path_draw(path, spline->points[2].x, spline->points[2].y);
    twin_paint_stroke(_apps_spline_pixmap(spline), 0xc08000c0, path,
                      twin_int_to_fixed(2));
    twin_path_empty(path);
    for (i = 0; i < NPT; i++) {
        twin_path_empty(path);
        twin_path_circle(path, spline->points[i].x, spline->points[i].y,
                         twin_int_to_fixed(10));
        twin_paint_path(_apps_spline_pixmap(spline), 0x40004020, path);
    }
    twin_path_destroy(path);
}

static twin_dispatch_result_t _apps_spline_update_pos(apps_spline_t *spline,
                                                      twin_event_t *event)
{
    if (spline->which < 0)
        return TwinDispatchContinue;
    spline->points[spline->which].x = twin_int_to_fixed(event->u.pointer.x);
    spline->points[spline->which].y = twin_int_to_fixed(event->u.pointer.y);
    _twin_widget_queue_paint(&spline->widget);
    return TwinDispatchDone;
}

#define twin_fixed_abs(f) ((f) < 0 ? -(f) : (f))

static int _apps_spline_hit(apps_spline_t *spline,
                            twin_fixed_t x,
                            twin_fixed_t y)
{
    int i;

    for (i = 0; i < NPT; i++)
        if (twin_fixed_abs(x - spline->points[i].x) < spline->line_width / 2 &&
            twin_fixed_abs(y - spline->points[i].y) < spline->line_width / 2)
            return i;
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
                              twin_dispatch_proc_t dispatch)
{
    static const twin_widget_layout_t preferred = {0, 0, 1, 1};
    _twin_widget_init(&spline->widget, parent, 0, preferred, dispatch);
    twin_widget_set(&spline->widget, 0xffffffff);
    spline->line_width = twin_int_to_fixed(100);
    spline->cap_style = TwinCapButt;
    spline->points[0].x = twin_int_to_fixed(100);
    spline->points[0].y = twin_int_to_fixed(100);
    spline->points[1].x = twin_int_to_fixed(300);
    spline->points[1].y = twin_int_to_fixed(300);
    spline->points[2].x = twin_int_to_fixed(100);
    spline->points[2].y = twin_int_to_fixed(300);
    spline->points[3].x = twin_int_to_fixed(300);
    spline->points[3].y = twin_int_to_fixed(100);
}

static apps_spline_t *apps_spline_create(twin_box_t *parent)
{
    apps_spline_t *spline = malloc(sizeof(apps_spline_t));

    _apps_spline_init(spline, parent, _apps_spline_dispatch);
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
    apps_spline_t *spline = apps_spline_create(&toplevel->box);
    (void) spline;
    twin_toplevel_show(toplevel);
}
