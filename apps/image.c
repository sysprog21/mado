/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

#include "apps_image.h"

#define _apps_image_pixmap(image) ((image)->widget.window->pixmap)

typedef struct {
    twin_widget_t widget;
    twin_pixmap_t *pix;
} apps_image_t;

static void _apps_image_paint(apps_image_t *img)
{
    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = img->pix,
    };
    twin_composite(_apps_image_pixmap(img), 0, 0, &srcop, 0, 0, NULL, 0, 0,
                   TWIN_SOURCE, img->pix->width, img->pix->height);
}

static twin_dispatch_result_t _apps_image_dispatch(twin_widget_t *widget,
                                                   twin_event_t *event)
{
    apps_image_t *img = (apps_image_t *) widget;
    if (_twin_widget_dispatch(widget, event) == TwinDispatchDone)
        return TwinDispatchDone;
    switch (event->kind) {
    case TwinEventPaint:
        _apps_image_paint(img);
        break;
    default:
        break;
    }
    return TwinDispatchContinue;
}

static void _apps_image_init(apps_image_t *img,
                             twin_box_t *parent,
                             twin_dispatch_proc_t dispatch)
{
    static const twin_widget_layout_t preferred = {0, 0, 1, 1};
    _twin_widget_init(&img->widget, parent, 0, preferred, dispatch);
}

static apps_image_t *apps_image_create(twin_box_t *parent, twin_pixmap_t *pix)
{
    apps_image_t *img = malloc(sizeof(apps_image_t));
    img->pix = pix;
    _apps_image_init(img, parent, _apps_image_dispatch);
    return img;
}

void apps_image_start(twin_screen_t *screen,
                      const char *name,
                      const char *path,
                      int x,
                      int y)
{
    twin_pixmap_t *pix = twin_pixmap_from_file(path, TWIN_ARGB32);
    if (!pix)
        return;
    twin_toplevel_t *toplevel =
        twin_toplevel_create(screen, TWIN_ARGB32, TwinWindowApplication, x, y,
                             pix->width, pix->height, name);
    apps_image_t *img = apps_image_create(&toplevel->box, pix);
    (void) img;
    twin_toplevel_show(toplevel);
}
