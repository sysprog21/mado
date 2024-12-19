/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

#include "apps_image.h"

#define _apps_image_pixmap(image) ((image)->widget.window->pixmap)
#define D(x) twin_double_to_fixed(x)
#define ASSET_PATH "assets/"
#define APP_WIDTH 400
#define APP_HEIGHT 400
typedef struct {
    twin_widget_t widget;
    twin_pixmap_t **pixes;
    int image_idx;
} apps_image_t;

static const char *tvg_files[] = {
    /* https://dev.w3.org/SVG/tools/svgweb/samples/svg-files/ */
    ASSET_PATH "tiger.tvg",
    /* https://tinyvg.tech/img/chart.svg */
    ASSET_PATH "chart.tvg",
    /* https://freesvg.org/betrayed */
    ASSET_PATH "comic.tvg",
    /* https://github.com/PapirusDevelopmentTeam/papirus-icon-theme */
    ASSET_PATH "folder.tvg",
    /* https://materialdesignicons.com/ */
    ASSET_PATH "shield.tvg",
    /* https://tinyvg.tech/img/flowchart.png */
    ASSET_PATH "flowchart.tvg",
};

static void _apps_image_paint(apps_image_t *img)
{
    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = img->pixes[img->image_idx],
    };

    twin_composite(_apps_image_pixmap(img), 0, 0, &srcop, 0, 0, NULL, 0, 0,
                   TWIN_SOURCE, APP_WIDTH, APP_HEIGHT);
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

static void _apps_image_button_signal(maybe_unused twin_button_t *button,
                                      twin_button_signal_t signal,
                                      void *closure)
{
    if (signal != TwinButtonSignalDown)
        return;

    apps_image_t *img = closure;
    const int n = sizeof(tvg_files) / sizeof(tvg_files[0]);
    img->image_idx = img->image_idx == n - 1 ? 0 : img->image_idx + 1;
    if (!img->pixes[img->image_idx]) {
        twin_pixmap_t *pix = twin_tvg_to_pixmap_scale(
            tvg_files[img->image_idx], TWIN_ARGB32, APP_WIDTH, APP_HEIGHT);
        if (!pix)
            return;
        img->pixes[img->image_idx] = pix;
    }
    _twin_widget_queue_paint(&img->widget);
}

static void _apps_image_init(apps_image_t *img,
                             twin_box_t *parent,
                             twin_dispatch_proc_t dispatch)
{
    static twin_widget_layout_t preferred = {0, 0, 1, 1};
    preferred.height = parent->widget.window->screen->height * 3.0 / 4.0;
    _twin_widget_init(&img->widget, parent, 0, preferred, dispatch);
    img->image_idx = 0;
    img->pixes = calloc(sizeof(tvg_files), sizeof(twin_pixmap_t *));
    img->pixes[0] = twin_tvg_to_pixmap_scale(tvg_files[0], TWIN_ARGB32,
                                             APP_WIDTH, APP_HEIGHT);
    twin_button_t *button =
        twin_button_create(parent, "Next Image", 0xFF482722, D(10),
                           TwinStyleBold | TwinStyleOblique);
    twin_widget_set(&button->label.widget, 0xFFFEE4CE);
    button->signal = _apps_image_button_signal;
    button->closure = img;
    button->label.widget.shape = TwinShapeRectangle;
}

static apps_image_t *apps_image_create(twin_box_t *parent)
{
    apps_image_t *img = malloc(sizeof(apps_image_t));

    _apps_image_init(img, parent, _apps_image_dispatch);
    return img;
}

void apps_image_start(twin_screen_t *screen, const char *name, int x, int y)
{
    twin_toplevel_t *toplevel =
        twin_toplevel_create(screen, TWIN_ARGB32, TwinWindowApplication, x, y,
                             APP_WIDTH, APP_HEIGHT, name);
    apps_image_t *img = apps_image_create(&toplevel->box);
    (void) img;
    twin_toplevel_show(toplevel);
}
