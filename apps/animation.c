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

    if (twin_pixmap_is_animated(anim->pix)) {
        twin_animation_t *a = anim->pix->animation;
        current_frame = twin_animation_get_current_frame(a);
        twin_animation_advance_frame(a);
    } else {
        current_frame = anim->pix;
    }

    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = current_frame,
    };
    twin_composite(_apps_animation_pixmap(custom), 0, 0, &srcop, 0, 0, NULL, 0,
                   0, TWIN_SOURCE, current_frame->width, current_frame->height);
}

static twin_time_t _apps_animation_timeout(twin_time_t now, void *closure)
{
    (void) now; /* unused parameter */
    twin_custom_widget_t *custom = closure;
    apps_animation_data_t *anim =
        (apps_animation_data_t *) twin_custom_widget_data(custom);
    twin_custom_widget_queue_paint(custom);
    twin_animation_t *a = anim->pix->animation;
    twin_time_t delay = twin_animation_get_current_delay(a);
    return delay;
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

    if (twin_pixmap_is_animated(anim->pix)) {
        twin_animation_t *a = anim->pix->animation;
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

void apps_animation_start(twin_screen_t *screen,
                          const char *name,
                          const char *path,
                          int x,
                          int y)
{
    twin_pixmap_t *pix = twin_pixmap_from_file(path, TWIN_ARGB32);
    twin_toplevel_t *toplevel =
        twin_toplevel_create(screen, TWIN_ARGB32, TwinWindowApplication, x, y,
                             pix->width, pix->height, name);
    twin_custom_widget_t *anim = apps_animation_create(&toplevel->box, pix);
    (void) anim;
    twin_toplevel_show(toplevel);
}
