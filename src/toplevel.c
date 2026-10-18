/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

twin_dispatch_result_t _twin_toplevel_dispatch(twin_widget_t *widget,
                                               twin_event_t *event,
                                               void *closure)
{
    twin_toplevel_t *toplevel = (twin_toplevel_t *) widget;
    twin_event_t ev = *event;

    switch (ev.kind) {
    case TwinEventConfigure:
        ev.u.configure.extents.left = 0;
        ev.u.configure.extents.top = 0;
        ev.u.configure.extents.right = (event->u.configure.extents.right -
                                        event->u.configure.extents.left);
        ev.u.configure.extents.bottom = (event->u.configure.extents.bottom -
                                         event->u.configure.extents.top);
        break;
    default:
        break;
    }
    return _twin_box_dispatch(&toplevel->box.widget, &ev, closure);
}

static bool _twin_toplevel_event(twin_window_t *window, twin_event_t *event)
{
    twin_toplevel_t *toplevel = window->client_data;

    return (*(&toplevel->box.widget)->handler)(&toplevel->box.widget, event,
                                               NULL) == TwinDispatchDone;
}

static void _twin_toplevel_draw(twin_window_t *window)
{
    twin_toplevel_t *toplevel = window->client_data;
    twin_event_t event;

    /* The window asks for its damaged pixels, which may be all of them (a
     * window manager restoring an evicted window): repaint every widget,
     * not only those queued since the last paint */
    _twin_box_mark_paint(&toplevel->box);
    twin_screen_disable_update(window->screen);
    event.kind = TwinEventPaint;
    (*(&toplevel->box.widget)->handler)(&toplevel->box.widget, &event, NULL);
    twin_screen_enable_update(window->screen);
}

static void _twin_toplevel_destroy(twin_window_t *window)
{
    twin_toplevel_t *toplevel = window->client_data;
    twin_event_t event;

    /* Mark this toplevel as being freed to prevent new work items */
    _twin_closure_mark_for_free(toplevel);

    /* Unregister from closure tracking */
    _twin_closure_unregister(toplevel);

    /* Refuse new paint/layout work while the widgets are torn down: the
     * Destroy event reaches application callbacks, which may touch widgets
     * and would otherwise queue work on a toplevel about to be freed. */
    toplevel->box.widget.paint = true;
    toplevel->box.widget.layout = true;

    event.kind = TwinEventDestroy;
    (*(&toplevel->box.widget)->handler)(&toplevel->box.widget, &event, NULL);

    /* Queued work must not outlive the toplevel: its address may be reused
     * by an object the tracker would then mistake for it */
    twin_clear_work(toplevel->paint_work);
    twin_clear_work(toplevel->layout_work);
    toplevel->paint_work = toplevel->layout_work = NULL;
    window->client_data = NULL;
    twin_free(toplevel);
}

void _twin_toplevel_init(twin_toplevel_t *toplevel,
                         twin_widget_proc_t dispatch,
                         twin_window_t *window,
                         const char *name)
{
    twin_window_set_name(window, name);
    window->draw = _twin_toplevel_draw;
    window->destroy = _twin_toplevel_destroy;
    window->event = _twin_toplevel_event;
    window->client_data = toplevel;
    _twin_box_init(&toplevel->box, 0, window, TwinBoxVert, dispatch);

    /* Register this toplevel with closure tracking */
    _twin_closure_register(toplevel);
}

twin_toplevel_t *twin_toplevel_create(twin_screen_t *screen,
                                      twin_format_t format,
                                      twin_window_style_t style,
                                      twin_coord_t x,
                                      twin_coord_t y,
                                      twin_coord_t width,
                                      twin_coord_t height,
                                      const char *name)
{
    twin_toplevel_t *toplevel;
    twin_window_t *window =
        twin_window_create(screen, format, style, x, y, width, height);

    if (!window)
        return NULL;
    toplevel = twin_calloc(1, sizeof(twin_toplevel_t) + strlen(name) + 1);
    if (!toplevel) {
        twin_window_destroy(window);
        return NULL;
    }
    _twin_toplevel_init(toplevel, _twin_toplevel_dispatch, window, name);
    return toplevel;
}

static bool _twin_toplevel_paint(void *closure)
{
    twin_toplevel_t *toplevel = closure;
    twin_event_t ev = {0};

    toplevel->paint_work = NULL;
    twin_screen_disable_update(toplevel->box.widget.window->screen);
    ev.kind = TwinEventPaint;
    (*(&toplevel->box.widget)->handler)(&toplevel->box.widget, &ev, NULL);
    twin_screen_enable_update(toplevel->box.widget.window->screen);
    return false;
}

void _twin_toplevel_queue_paint(twin_widget_t *widget)
{
    twin_toplevel_t *toplevel = (twin_toplevel_t *) widget;

    if (!toplevel->box.widget.paint) {
        toplevel->box.widget.paint = true;
        toplevel->paint_work =
            twin_set_work(_twin_toplevel_paint, TWIN_WORK_PAINT, toplevel);
    }
}

static bool _twin_toplevel_layout(void *closure)
{
    twin_toplevel_t *toplevel = closure;
    twin_event_t ev = {0};
    twin_window_t *window = toplevel->box.widget.window;

    toplevel->layout_work = NULL;

    ev.kind = TwinEventQueryGeometry;
    (*(&toplevel->box.widget)->handler)(&toplevel->box.widget, &ev, NULL);
    ev.kind = TwinEventConfigure;
    ev.u.configure.extents.left = 0;
    ev.u.configure.extents.top = 0;
    ev.u.configure.extents.right = window->client.right - window->client.left;
    ev.u.configure.extents.bottom = window->client.bottom - window->client.top;
    (*(&toplevel->box.widget)->handler)(&toplevel->box.widget, &ev, NULL);
    return false;
}

void _twin_toplevel_queue_layout(twin_widget_t *widget)
{
    twin_toplevel_t *toplevel = (twin_toplevel_t *) widget;

    if (!toplevel->box.widget.layout) {
        toplevel->box.widget.layout = true;
        toplevel->layout_work =
            twin_set_work(_twin_toplevel_layout, TWIN_WORK_LAYOUT, toplevel);
        _twin_toplevel_queue_paint(widget);
    }
}

void twin_toplevel_show(twin_toplevel_t *toplevel)
{
    _twin_toplevel_layout(toplevel);
    _twin_toplevel_paint(toplevel);
    twin_window_show(toplevel->box.widget.window);
}

twin_box_t *twin_toplevel_box(twin_toplevel_t *toplevel)
{
    return &toplevel->box;
}
