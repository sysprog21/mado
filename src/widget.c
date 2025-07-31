/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

static twin_path_t *_twin_path_shape(twin_shape_t shape,
                                     twin_coord_t left,
                                     twin_coord_t top,
                                     twin_coord_t right,
                                     twin_coord_t bottom,
                                     twin_fixed_t radius)
{
    twin_path_t *path = twin_path_create();
    if (!path)
        return NULL;

    twin_fixed_t x = twin_int_to_fixed(left);
    twin_fixed_t y = twin_int_to_fixed(top);
    twin_fixed_t w = twin_int_to_fixed(right - left);
    twin_fixed_t h = twin_int_to_fixed(bottom - top);

    switch (shape) {
    case TwinShapeRectangle:
        twin_path_rectangle(path, x, y, w, h);
        break;
    case TwinShapeRoundedRectangle:
        twin_path_rounded_rectangle(path, x, h, w, y, radius, radius);
        break;
    case TwinShapeLozenge:
        twin_path_lozenge(path, x, y, w, h);
        break;
    case TwinShapeTab:
        twin_path_tab(path, x, y, w, h, radius, radius);
        break;
    case TwinShapeEllipse:
        twin_path_ellipse(path, x + w / 2, y + h / 2, w / 2, h / 2);
        break;
    }
    return path;
}

void _twin_widget_paint_shape(twin_widget_t *widget,
                              twin_shape_t shape,
                              twin_coord_t left,
                              twin_coord_t top,
                              twin_coord_t right,
                              twin_coord_t bottom,
                              twin_fixed_t radius)
{
    twin_pixmap_t *pixmap = widget->window->pixmap;

    if (shape == TwinShapeRectangle)
        twin_fill(pixmap, widget->background, TWIN_SOURCE, left, top, right,
                  bottom);
    else {
        twin_path_t *path =
            _twin_path_shape(shape, left, top, right, bottom, radius);
        if (path) {
            twin_paint_path(pixmap, widget->background, path);
            twin_path_destroy(path);
        }
    }
}

static void _twin_widget_paint(twin_widget_t *widget)
{
    _twin_widget_paint_shape(widget, widget->shape, 0, 0,
                             _twin_widget_width(widget),
                             _twin_widget_height(widget), widget->radius);
}

twin_dispatch_result_t _twin_widget_dispatch(twin_widget_t *widget,
                                             twin_event_t *event)
{
    switch (event->kind) {
    case TwinEventQueryGeometry:
        widget->layout = false;
        if (widget->copy_geom) {
            twin_widget_t *copy = widget->copy_geom;
            if (copy->layout)
                (*copy->dispatch)(copy, event);
            widget->preferred = copy->preferred;
            return TwinDispatchDone;
        }
        break;
    case TwinEventConfigure:
        widget->extents = event->u.configure.extents;
        break;
    case TwinEventPaint:
        _twin_widget_paint(widget);
        widget->paint = false;
        break;
    case TwinEventDestroy:
        /* Base widget has no special cleanup */
        break;
    default:
        break;
    }
    return TwinDispatchContinue;
}

void _twin_widget_init(twin_widget_t *widget,
                       twin_box_t *parent,
                       twin_window_t *window,
                       twin_widget_layout_t preferred,
                       twin_dispatch_proc_t dispatch)
{
    if (parent) {
        twin_widget_t **prev;

        for (prev = &parent->children; *prev; prev = &(*prev)->next)
            ;
        widget->next = *prev;
        *prev = widget;
        window = parent->widget.window;
    } else
        widget->next = NULL;

    widget->window = window;
    widget->parent = parent;
    widget->copy_geom = NULL;
    widget->paint = true;
    widget->layout = true;
    widget->want_focus = false;
    widget->background = 0x00000000;
    widget->extents.left = widget->extents.top = 0;
    widget->extents.right = widget->extents.bottom = 0;
    widget->preferred = preferred;
    widget->dispatch = dispatch;
    widget->shape = TwinShapeRectangle;
    widget->radius = twin_int_to_fixed(12);
}

void _twin_widget_queue_paint(twin_widget_t *widget)
{
    while (widget->parent) {
        if (widget->paint)
            return;

        widget->paint = true;
        widget = &widget->parent->widget;
    }
    _twin_toplevel_queue_paint(widget);
}

void _twin_widget_queue_layout(twin_widget_t *widget)
{
    while (widget->parent) {
        if (widget->layout)
            return;

        widget->layout = true;
        widget->paint = true;
        widget = &widget->parent->widget;
    }
    _twin_toplevel_queue_layout(widget);
}

bool _twin_widget_contains(twin_widget_t *widget,
                           twin_coord_t x,
                           twin_coord_t y)
{
    return (0 <= x && x < _twin_widget_width(widget) && 0 <= y &&
            y < _twin_widget_height(widget));
}

void _twin_widget_bevel(twin_widget_t *widget, twin_fixed_t b, bool down)
{
    twin_path_t *path = twin_path_create();
    twin_fixed_t w = twin_int_to_fixed(_twin_widget_width(widget));
    twin_fixed_t h = twin_int_to_fixed(_twin_widget_height(widget));
    twin_argb32_t top_color, bot_color;
    twin_pixmap_t *pixmap = widget->window->pixmap;

    if (path) {
        if (down) {
            top_color = 0x80000000;
            bot_color = 0x80808080;
        } else {
            top_color = 0x80808080;
            bot_color = 0x80000000;
        }

        twin_path_move(path, 0, 0);
        twin_path_draw(path, w, 0);
        twin_path_draw(path, w - b, b);
        twin_path_draw(path, b, b);
        twin_path_draw(path, b, h - b);
        twin_path_draw(path, 0, h);
        twin_path_close(path);
        twin_paint_path(pixmap, top_color, path);
        twin_path_empty(path);
        twin_path_move(path, b, h - b);
        twin_path_draw(path, w - b, h - b);
        twin_path_draw(path, w - b, b);
        twin_path_draw(path, w, 0);
        twin_path_draw(path, w, h);
        twin_path_draw(path, 0, h);
        twin_path_close(path);
        twin_paint_path(pixmap, bot_color, path);
        twin_path_destroy(path);
    }
}

void twin_widget_children_paint(twin_box_t *box)
{
    for (twin_widget_t *child = box->children; child; child = child->next)
        _twin_widget_queue_paint(child);
}

twin_widget_t *twin_widget_create(twin_box_t *parent,
                                  twin_argb32_t background,
                                  twin_coord_t width,
                                  twin_coord_t height,
                                  twin_stretch_t stretch_width,
                                  twin_stretch_t stretch_height)
{
    twin_widget_t *widget = malloc(sizeof(twin_widget_t));
    if (!widget)
        return NULL;

    twin_widget_layout_t preferred = {
        .width = width,
        .height = height,
        .stretch_width = stretch_width,
        .stretch_height = stretch_height,
    };
    _twin_widget_init(widget, parent, 0, preferred, _twin_widget_dispatch);
    widget->background = background;
    return widget;
}

void twin_widget_set(twin_widget_t *widget, twin_argb32_t background)
{
    widget->background = background;
    _twin_widget_queue_paint(widget);
}

twin_fixed_t twin_widget_width(twin_widget_t *widget)
{
    return _twin_widget_width(widget);
}

twin_fixed_t twin_widget_height(twin_widget_t *widget)
{
    return _twin_widget_height(widget);
}

void twin_widget_queue_paint(twin_widget_t *widget)
{
    _twin_widget_queue_paint(widget);
}

twin_widget_t *twin_widget_create_with_dispatch(twin_box_t *parent,
                                                twin_argb32_t background,
                                                twin_coord_t width,
                                                twin_coord_t height,
                                                twin_stretch_t stretch_width,
                                                twin_stretch_t stretch_height,
                                                twin_dispatch_proc_t dispatch)
{
    twin_widget_t *widget = malloc(sizeof(twin_widget_t));
    if (!widget)
        return NULL;

    twin_widget_layout_t preferred = {
        .width = width,
        .height = height,
        .stretch_width = stretch_width,
        .stretch_height = stretch_height,
    };
    _twin_widget_init(widget, parent, 0, preferred, dispatch);
    widget->background = background;
    return widget;
}

/* Map to store custom widget associations */
typedef struct _custom_widget_map {
    twin_widget_t *widget;
    twin_custom_widget_t *custom;
    twin_dispatch_proc_t user_dispatch;
    struct _custom_widget_map *next;
} custom_widget_map_t;

static custom_widget_map_t *custom_widget_map = NULL;

static void register_custom_widget(twin_widget_t *widget,
                                   twin_custom_widget_t *custom,
                                   twin_dispatch_proc_t user_dispatch)
{
    custom_widget_map_t *entry = malloc(sizeof(custom_widget_map_t));
    if (!entry)
        return;
    entry->widget = widget;
    entry->custom = custom;
    entry->user_dispatch = user_dispatch;
    entry->next = custom_widget_map;
    custom_widget_map = entry;
}

static twin_custom_widget_t *find_custom_widget(twin_widget_t *widget)
{
    custom_widget_map_t *entry = custom_widget_map;
    while (entry) {
        if (entry->widget == widget)
            return entry->custom;
        entry = entry->next;
    }
    return NULL;
}

static void unregister_custom_widget(twin_widget_t *widget)
{
    custom_widget_map_t **prev = &custom_widget_map;
    custom_widget_map_t *entry = custom_widget_map;

    while (entry) {
        if (entry->widget == widget) {
            *prev = entry->next;
            free(entry);
            return;
        }
        prev = &entry->next;
        entry = entry->next;
    }
}

static twin_dispatch_result_t custom_widget_dispatch(twin_widget_t *widget,
                                                     twin_event_t *event)
{
    /* First call the base widget dispatch to handle standard widget behavior */
    twin_dispatch_result_t result = _twin_widget_dispatch(widget, event);
    if (result == TwinDispatchDone)
        return result;

    /* Then call the user's custom dispatch if any */
    custom_widget_map_t *entry = custom_widget_map;
    while (entry) {
        if (entry->widget == widget) {
            if (entry->user_dispatch)
                return entry->user_dispatch(widget, event);
            break;
        }
        entry = entry->next;
    }
    return TwinDispatchContinue;
}

twin_custom_widget_t *twin_custom_widget_create(twin_box_t *parent,
                                                twin_argb32_t background,
                                                twin_coord_t width,
                                                twin_coord_t height,
                                                twin_stretch_t hstretch,
                                                twin_stretch_t vstretch,
                                                twin_dispatch_proc_t dispatch,
                                                size_t data_size)
{
    twin_custom_widget_t *custom = malloc(sizeof(twin_custom_widget_t));
    if (!custom)
        return NULL;

    if (data_size > 0) {
        custom->data = malloc(data_size);
        if (!custom->data) {
            free(custom);
            return NULL;
        }
        memset(custom->data, 0, data_size);
    } else {
        custom->data = NULL;
    }

    custom->widget = twin_widget_create_with_dispatch(
        parent, background, width, height, hstretch, vstretch,
        custom_widget_dispatch);
    if (!custom->widget) {
        free(custom->data);
        free(custom);
        return NULL;
    }

    register_custom_widget(custom->widget, custom, dispatch);

    return custom;
}

void twin_custom_widget_destroy(twin_custom_widget_t *custom)
{
    if (!custom)
        return;

    if (custom->widget) {
        unregister_custom_widget(custom->widget);
        /* Note: widget destruction should be handled by the parent/container */
    }
    free(custom->data);
    free(custom);
}

twin_pixmap_t *twin_widget_pixmap(twin_widget_t *widget)
{
    if (!widget || !widget->window)
        return NULL;
    return widget->window->pixmap;
}

twin_custom_widget_t *twin_widget_get_custom(twin_widget_t *widget)
{
    return find_custom_widget(widget);
}

void *twin_custom_widget_data(twin_custom_widget_t *custom)
{
    return custom ? custom->data : NULL;
}

twin_widget_t *twin_custom_widget_base(twin_custom_widget_t *custom)
{
    return custom ? custom->widget : NULL;
}

twin_fixed_t twin_custom_widget_width(twin_custom_widget_t *custom)
{
    return custom ? twin_widget_width(custom->widget) : 0;
}

twin_fixed_t twin_custom_widget_height(twin_custom_widget_t *custom)
{
    return custom ? twin_widget_height(custom->widget) : 0;
}

void twin_custom_widget_queue_paint(twin_custom_widget_t *custom)
{
    if (custom && custom->widget)
        twin_widget_queue_paint(custom->widget);
}

twin_pixmap_t *twin_custom_widget_pixmap(twin_custom_widget_t *custom)
{
    return custom ? twin_widget_pixmap(custom->widget) : NULL;
}
