/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University
 * All rights reserved.
 */

#include <stdlib.h>

#include <twin.h>

#include "apps.h"

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
    /* Paint the current frame: an animation redraws its pixmap in place,
     * and only the timer advances it, so repainting (exposure, a restore
     * after eviction) does not skip frames */
    twin_pixmap_t *current_frame = anim->pix;

    /* A policy may give the window more room than the image (a phone's
     * work area, the whole screen without a window manager): center it */
    twin_pixmap_t *dst = _apps_animation_pixmap(custom);
    twin_coord_t w = twin_custom_widget_width(custom);
    twin_coord_t h = twin_custom_widget_height(custom);
    twin_coord_t fw = twin_pixmap_width(current_frame);
    twin_coord_t fh = twin_pixmap_height(current_frame);
    twin_coord_t x = w > fw ? (w - fw) / 2 : 0;
    twin_coord_t y = h > fh ? (h - fh) / 2 : 0;

    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = current_frame,
    };
    /* Over the cleared widget: a GIF's transparent pixels stay clear, and
     * whatever is behind the window shows through them */
    twin_composite(dst, x, y, &srcop, 0, 0, NULL, 0, 0, TWIN_OVER, fw, fh);
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

static twin_dispatch_result_t _apps_animation_dispatch(
    twin_custom_widget_t *custom,
    twin_event_t *event)
{
    switch (event->kind) {
    case TwinEventPaint:
        _apps_animation_paint(custom);
        break;
    case TwinEventDestroy: {
        /* The pixmap owns the animation, so this frees the decoder too */
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
        parent, 0x00000000, 0, 0, _apps_animation_dispatch,
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

/* Sized by the image: @w and @h are ignored */
bool apps_animation_start(twin_screen_t *screen,
                          const char *name,
                          int x,
                          int y,
                          int w,
                          int h)
{
    (void) w, (void) h;
    twin_pixmap_t *pix = twin_pixmap_from_file(ASSET_PATH "nyancat.gif");
    if (!pix)
        return false;
    twin_toplevel_t *toplevel = twin_toplevel_create(
        screen, x, y, twin_pixmap_width(pix), twin_pixmap_height(pix), name);
    if (!toplevel) {
        twin_pixmap_destroy(pix);
        return false;
    }
    /* See-through: a clear box makes the window translucent, so the GIF's
     * transparency reaches the screen instead of a solid fill */
    twin_widget_set(twin_box_widget(twin_toplevel_box(toplevel)), 0x00000000);
    /* The widget owns the image from here on */
    if (!apps_animation_create(twin_toplevel_box(toplevel), pix)) {
        twin_toplevel_destroy(toplevel);
        twin_pixmap_destroy(pix);
        return false;
    }
    twin_toplevel_show(toplevel);
    return true;
}
