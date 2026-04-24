/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University
 * All rights reserved.
 */

#include <stdlib.h>

#include <twin.h>

#include "apps_image.h"

static inline twin_pixmap_t *_apps_image_pixmap(twin_custom_widget_t *image)
{
    return twin_custom_widget_pixmap(image);
}

#define D(x) twin_double_to_fixed(x)
#define ASSET_PATH "assets/"
#define APP_WIDTH 400
#define APP_HEIGHT 400

static twin_pixmap_t *load_tvg(const char *path)
{
#if defined(CONFIG_TVG_MEMORY_BUDGET) && CONFIG_TVG_MEMORY_BUDGET > 0
    return twin_tvg_to_pixmap_budget(path, TWIN_ARGB32,
                                     (size_t) CONFIG_TVG_MEMORY_BUDGET * 1024);
#else
    return twin_tvg_to_pixmap_scale(path, TWIN_ARGB32, APP_WIDTH, APP_HEIGHT);
#endif
}

typedef struct {
    twin_pixmap_t *current_pix; /* only the visible image is kept */
    int image_idx;
} apps_image_data_t;

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

static void _apps_image_paint(twin_custom_widget_t *custom)
{
    apps_image_data_t *img =
        (apps_image_data_t *) twin_custom_widget_data(custom);
    if (!img->current_pix)
        return;
    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = img->current_pix,
    };

    twin_composite(_apps_image_pixmap(custom), 0, 0, &srcop, 0, 0, NULL, 0, 0,
                   TWIN_SOURCE, img->current_pix->width,
                   img->current_pix->height);
}

static twin_dispatch_result_t _apps_image_dispatch(twin_widget_t *widget,
                                                   twin_event_t *event,
                                                   void *closure)
{
    (void) closure; /* unused parameter */

    twin_custom_widget_t *custom = twin_widget_get_custom(widget);
    if (!custom)
        return TwinDispatchContinue;

    switch (event->kind) {
    case TwinEventPaint:
        _apps_image_paint(custom);
        break;
    case TwinEventDestroy: {
        apps_image_data_t *img =
            (apps_image_data_t *) twin_custom_widget_data(custom);
        if (img->current_pix) {
            twin_pixmap_destroy(img->current_pix);
            img->current_pix = NULL;
        }
        break;
    }
    default:
        break;
    }
    return TwinDispatchContinue;
}

static twin_dispatch_result_t _apps_image_button_clicked(twin_widget_t *widget,
                                                         twin_event_t *event,
                                                         void *data)
{
    (void) widget; /* unused parameter */

    if (event->kind != TwinEventButtonSignalUp)
        return TwinDispatchContinue;

    twin_custom_widget_t *custom = data;
    apps_image_data_t *img =
        (apps_image_data_t *) twin_custom_widget_data(custom);
    const int n = sizeof(tvg_files) / sizeof(tvg_files[0]);
    img->image_idx = img->image_idx == n - 1 ? 0 : img->image_idx + 1;

    /* Evict the old pixmap before loading the new one */
    if (img->current_pix) {
        twin_pixmap_destroy(img->current_pix);
        img->current_pix = NULL;
    }
    img->current_pix = load_tvg(tvg_files[img->image_idx]);
    if (!img->current_pix)
        return TwinDispatchContinue;
    twin_custom_widget_queue_paint(custom);
    return TwinDispatchDone;
}

static twin_custom_widget_t *_apps_image_init(twin_box_t *parent)
{
    twin_custom_widget_t *custom = twin_custom_widget_create(
        parent, 0, 0, parent->widget.window->screen->height * 3 / 4, 1, 1,
        _apps_image_dispatch, sizeof(apps_image_data_t));
    if (!custom)
        return NULL;

    apps_image_data_t *img =
        (apps_image_data_t *) twin_custom_widget_data(custom);
    img->image_idx = 0;
    img->current_pix = load_tvg(tvg_files[0]);

    twin_button_t *button =
        twin_button_create(parent, "Next Image", 0xFF482722, D(10),
                           TwinStyleBold | TwinStyleOblique);
    twin_widget_set(&button->label.widget, 0xFFFEE4CE);
    twin_widget_set_callback(&button->label.widget, _apps_image_button_clicked,
                             custom);
    button->label.widget.shape = TwinShapeRectangle;

    return custom;
}

static twin_custom_widget_t *apps_image_create(twin_box_t *parent)
{
    return _apps_image_init(parent);
}

void apps_image_start(twin_screen_t *screen, const char *name, int x, int y)
{
    twin_toplevel_t *toplevel =
        twin_toplevel_create(screen, TWIN_ARGB32, TwinWindowApplication, x, y,
                             APP_WIDTH, APP_HEIGHT, name);
    twin_custom_widget_t *img = apps_image_create(&toplevel->box);
    (void) img;
    twin_toplevel_show(toplevel);
}
