/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

#include "apps_animation.h"

#define _apps_animation_pixmap(animation) ((animation)->widget.window->pixmap)

typedef struct {
    twin_widget_t widget;
    twin_pixmap_t *pix;
    twin_timeout_t *timeout;
} apps_animation_t;

static void _apps_animation_paint(apps_animation_t *anim)
{
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
    twin_composite(_apps_animation_pixmap(anim), 0, 0, &srcop, 0, 0, NULL, 0, 0,
                   TWIN_SOURCE, current_frame->width, current_frame->height);
}

static twin_time_t _apps_animation_timeout(twin_time_t maybe_unused now,
                                           void *closure)
{
    apps_animation_t *anim = closure;
    _twin_widget_queue_paint(&anim->widget);
    twin_animation_t *a = anim->pix->animation;
    twin_time_t delay = twin_animation_get_current_delay(a);
    return delay;
}

static twin_dispatch_result_t _apps_animation_dispatch(twin_widget_t *widget,
                                                       twin_event_t *event)
{
    apps_animation_t *anim = (apps_animation_t *) widget;
    if (_twin_widget_dispatch(widget, event) == TwinDispatchDone)
        return TwinDispatchDone;
    switch (event->kind) {
    case TwinEventPaint:
        _apps_animation_paint(anim);
        break;
    default:
        break;
    }
    return TwinDispatchContinue;
}

static void _apps_animation_init(apps_animation_t *anim,
                                 twin_box_t *parent,
                                 twin_dispatch_proc_t dispatch)
{
    static const twin_widget_layout_t preferred = {0, 0, 1, 1};
    _twin_widget_init(&anim->widget, parent, 0, preferred, dispatch);

    if (twin_pixmap_is_animated(anim->pix)) {
        twin_animation_t *a = anim->pix->animation;
        twin_time_t delay = twin_animation_get_current_delay(a);
        anim->timeout = twin_set_timeout(_apps_animation_timeout, delay, anim);
    } else {
        anim->timeout = NULL;
    }
}

static apps_animation_t *apps_animation_create(twin_box_t *parent,
                                               twin_pixmap_t *pix)
{
    apps_animation_t *anim = malloc(sizeof(apps_animation_t));
    anim->pix = pix;
    _apps_animation_init(anim, parent, _apps_animation_dispatch);
    return anim;
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
    apps_animation_t *anim = apps_animation_create(&toplevel->box, pix);
    (void) anim;
    twin_toplevel_show(toplevel);
}
