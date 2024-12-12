/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>

#include "twin_private.h"

#include "apps_line.h"

#define D(x) twin_double_to_fixed(x)

#define _apps_line_pixmap(line) ((line)->widget.window->pixmap)

typedef struct _apps_line {
    twin_widget_t widget;
    twin_point_t points[2];
    int which;
    twin_fixed_t line_width;
    twin_cap_t cap_style;
} apps_line_t;

static void _apps_line_paint(apps_line_t *line)
{
    twin_path_t *path;

    path = twin_path_create();
    twin_path_set_cap_style(path, line->cap_style);
    twin_path_move(path, line->points[0].x, line->points[0].y);
    twin_path_draw(path, line->points[1].x, line->points[1].y);
    twin_paint_stroke(_apps_line_pixmap(line), 0xff000000, path,
                      line->line_width);
    twin_path_set_cap_style(path, TwinCapButt);
    twin_paint_stroke(_apps_line_pixmap(line), 0xffff0000, path,
                      twin_int_to_fixed(2));
    twin_path_destroy(path);
}

static twin_dispatch_result_t _apps_line_update_pos(apps_line_t *line,
                                                    twin_event_t *event)
{
    if (line->which < 0)
        return TwinDispatchContinue;
    line->points[line->which].x = twin_int_to_fixed(event->u.pointer.x);
    line->points[line->which].y = twin_int_to_fixed(event->u.pointer.y);
    _twin_widget_queue_paint(&line->widget);
    return TwinDispatchDone;
}

static int _apps_line_hit(apps_line_t *line, twin_fixed_t x, twin_fixed_t y)
{
    int i;

    for (i = 0; i < 2; i++)
        if (twin_fixed_abs(x - line->points[i].x) < line->line_width / 2 &&
            twin_fixed_abs(y - line->points[i].y) < line->line_width / 2)
            return i;
    return -1;
}

static twin_dispatch_result_t _apps_line_dispatch(twin_widget_t *widget,
                                                  twin_event_t *event)
{
    apps_line_t *line = (apps_line_t *) widget;

    if (_twin_widget_dispatch(widget, event) == TwinDispatchDone)
        return TwinDispatchDone;
    switch (event->kind) {
    case TwinEventPaint:
        _apps_line_paint(line);
        break;
    case TwinEventButtonDown:
        line->which =
            _apps_line_hit(line, twin_int_to_fixed(event->u.pointer.x),
                           twin_int_to_fixed(event->u.pointer.y));
        return _apps_line_update_pos(line, event);
        break;
    case TwinEventMotion:
        return _apps_line_update_pos(line, event);
        break;
    case TwinEventButtonUp:
        if (line->which < 0)
            return TwinDispatchContinue;
        _apps_line_update_pos(line, event);
        line->which = -1;
        return TwinDispatchDone;
        break;
    default:
        break;
    }
    return TwinDispatchContinue;
}

static void _apps_line_init(apps_line_t *line,
                            twin_box_t *parent,
                            twin_dispatch_proc_t dispatch)
{
    static const twin_widget_layout_t preferred = {0, 0, 1, 1};
    _twin_widget_init(&line->widget, parent, 0, preferred, dispatch);
    twin_widget_set(&line->widget, 0xffffffff);
    line->line_width = twin_int_to_fixed(30);
    line->cap_style = TwinCapProjecting;
    line->points[0].x = twin_int_to_fixed(50);
    line->points[0].y = twin_int_to_fixed(50);
    line->points[1].x = twin_int_to_fixed(100);
    line->points[1].y = twin_int_to_fixed(100);
}

static apps_line_t *apps_line_create(twin_box_t *parent)
{
    apps_line_t *line = malloc(sizeof(apps_line_t));

    _apps_line_init(line, parent, _apps_line_dispatch);
    return line;
}

void apps_line_start(twin_screen_t *screen,
                     const char *name,
                     int x,
                     int y,
                     int w,
                     int h)
{
    twin_toplevel_t *toplevel = twin_toplevel_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, name);
    apps_line_t *line = apps_line_create(&toplevel->box);
    (void) line;
    twin_toplevel_show(toplevel);
}
