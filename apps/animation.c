/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University
 * All rights reserved.
 */

#include <stdlib.h>

#include <twin.h>

#include "apps_animation.h"

static inline twin_pixmap_t *_apps_animation_pixmap(
    twin_custom_widget_t *animation)
{
    return twin_custom_widget_pixmap(animation);
}

typedef struct {
    twin_pixmap_t *pix;
    twin_timeout_t *timeout;
} apps_animation_data_t;

static void _apps_animation_paint(twin_custom_widget_t *custom)
{
    apps_animation_data_t *anim =
        (apps_animation_data_t *) twin_custom_widget_data(custom);
    twin_pixmap_t *current_frame = NULL;

    /* Paint the current frame; only the timer advances it, so repainting
     * (exposure, a restore after eviction) does not skip frames */
    twin_animation_t *a = twin_pixmap_animation(anim->pix);
    current_frame = a ? twin_animation_get_current_frame(a) : anim->pix;

    /* A policy may give the window more room than the image (a phone's
     * work area, the whole screen without a window manager): center it */
    twin_pixmap_t *dst = _apps_animation_pixmap(custom);
    twin_coord_t w = twin_custom_widget_width(custom);
    twin_coord_t h = twin_custom_widget_height(custom);
    twin_coord_t fw = twin_pixmap_width(current_frame);
    twin_coord_t fh = twin_pixmap_height(current_frame);
    twin_coord_t x = w > fw ? (w - fw) / 2 : 0;
    twin_coord_t y = h > fh ? (h - fh) / 2 : 0;
    if (w > fw || h > fh)
        twin_fill(dst, 0xff000000, TWIN_SOURCE, 0, 0, w, h);

    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = current_frame,
    };
    twin_composite(dst, x, y, &srcop, 0, 0, NULL, 0, 0, TWIN_SOURCE, fw, fh);
}

static twin_time_t _apps_animation_timeout(twin_time_t now, void *closure)
{
    (void) now; /* unused parameter */
    twin_custom_widget_t *custom = closure;
    apps_animation_data_t *anim =
        (apps_animation_data_t *) twin_custom_widget_data(custom);
    twin_animation_t *a = twin_pixmap_animation(anim->pix);
    twin_animation_advance_frame(a);
    twin_custom_widget_queue_paint(custom);
    return twin_animation_get_current_delay(a);
}

static twin_dispatch_result_t _apps_animation_dispatch(twin_widget_t *widget,
                                                       twin_event_t *event,
                                                       void *closure)
{
    (void) closure; /* unused parameter */

    twin_custom_widget_t *custom = twin_widget_get_custom(widget);
    if (!custom)
        return TwinDispatchContinue;

    switch (event->kind) {
    case TwinEventPaint:
        _apps_animation_paint(custom);
        break;
    case TwinEventDestroy: {
        /* The pixmap owns the animation, so this frees every frame too */
        apps_animation_data_t *anim =
            (apps_animation_data_t *) twin_custom_widget_data(custom);
        twin_clear_timeout(anim->timeout);
        anim->timeout = NULL;
        twin_pixmap_destroy(anim->pix);
        anim->pix = NULL;
        break;
    }
    default:
        break;
    }
    return TwinDispatchContinue;
}

static twin_custom_widget_t *_apps_animation_init(twin_box_t *parent,
                                                  twin_pixmap_t *pix)
{
    twin_custom_widget_t *custom = twin_custom_widget_create(
        parent, 0, 0, 0, 1, 1, _apps_animation_dispatch,
        sizeof(apps_animation_data_t));
    if (!custom)
        return NULL;

    apps_animation_data_t *anim =
        (apps_animation_data_t *) twin_custom_widget_data(custom);
    anim->pix = pix;

    twin_animation_t *a = twin_pixmap_animation(anim->pix);
    if (a) {
        twin_time_t delay = twin_animation_get_current_delay(a);
        anim->timeout =
            twin_set_timeout(_apps_animation_timeout, delay, custom);
    } else {
        anim->timeout = NULL;
    }

    return custom;
}

static twin_custom_widget_t *apps_animation_create(twin_box_t *parent,
                                                   twin_pixmap_t *pix)
{
    return _apps_animation_init(parent, pix);
}

bool apps_animation_start(twin_screen_t *screen,
                          const char *name,
                          const char *path,
                          int x,
                          int y)
{
    twin_pixmap_t *pix = twin_pixmap_from_file(path, TWIN_ARGB32);
    if (!pix)
        return false;
    twin_toplevel_t *toplevel = twin_toplevel_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y,
        twin_pixmap_width(pix), twin_pixmap_height(pix), name);
    if (!toplevel) {
        twin_pixmap_destroy(pix);
        return false;
    }
    /* The widget owns the image from here on */
    if (!apps_animation_create(twin_toplevel_box(toplevel), pix)) {
        twin_toplevel_destroy(toplevel);
        twin_pixmap_destroy(pix);
        return false;
    }
    twin_toplevel_show(toplevel);
    return true;
}
