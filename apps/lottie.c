/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 *
 * Lottie player with Play/Pause and Loop controls
 */

#include <stdlib.h>

#include <twin.h>

#include "apps_lottie.h"

#define BUTTON_SIZE twin_int_to_fixed(8)

typedef struct {
    mado_lottie_image_t *lottie;
    twin_pixmap_t *pix;
    twin_timeout_t *timeout;
    twin_label_t *status;
    twin_custom_widget_t *display;
} lottie_app_t;

/*
 * Animation display
 */

static void _lottie_paint(twin_custom_widget_t *widget)
{
    lottie_app_t *app = (lottie_app_t *)twin_custom_widget_data(widget);
    if (!app || !app->pix || !app->pix->animation)
        return;

    twin_pixmap_t *dst = twin_custom_widget_pixmap(widget);
    if (!dst)
        return;

    twin_pixmap_t *frame = twin_animation_get_current_frame(app->pix->animation);
    if (!frame)
        return;

    twin_fill(dst, 0xffe0e0e0, TWIN_SOURCE, 0, 0, dst->width, dst->height);

    twin_coord_t ox = (dst->width - frame->width) / 2;
    twin_coord_t oy = (dst->height - frame->height) / 2;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;

    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = frame,
    };
    twin_composite(dst, ox, oy, &srcop, 0, 0, NULL, 0, 0,
                   TWIN_OVER, frame->width, frame->height);
}

static twin_dispatch_result_t _lottie_dispatch(twin_widget_t *widget,
                                               twin_event_t *event,
                                               void *closure)
{
    (void)closure;
    twin_custom_widget_t *cw = twin_widget_get_custom(widget);
    if (cw && event->kind == TwinEventPaint)
        _lottie_paint(cw);
    return TwinDispatchContinue;
}

/*
 * Timer
 */

static twin_time_t _lottie_timeout(twin_time_t now, void *closure)
{
    (void)now;
    lottie_app_t *app = closure;

    if (!app || !app->lottie || !app->pix)
        return 100;

    if (mado_lottie_is_playing(app->lottie)) {
        twin_animation_advance_frame(app->pix->animation);
        if (app->display)
            twin_custom_widget_queue_paint(app->display);
    }

    return mado_lottie_get_frame_delay(app->lottie);
}

/*
 * Status update
 */

static void update_status(lottie_app_t *app)
{
    if (!app->status || !app->lottie)
        return;

    bool playing = mado_lottie_is_playing(app->lottie);
    bool loop = mado_lottie_is_looping(app->lottie);

    const char *text;
    if (playing)
        text = loop ? "Playing | Loop" : "Playing";
    else
        text = loop ? "Paused | Loop" : "Paused";

    twin_label_set(app->status, text, 0xff000000, BUTTON_SIZE, TwinStyleBold);
}

/*
 * Button callbacks
 */

static twin_dispatch_result_t _play_cb(twin_widget_t *widget,
                                       twin_event_t *event,
                                       void *closure)
{
    (void)widget;
    if (event->kind != TwinEventButtonSignalUp)
        return TwinDispatchContinue;

    lottie_app_t *app = closure;
    mado_lottie_set_playback(app->lottie, !mado_lottie_is_playing(app->lottie));
    update_status(app);
    return TwinDispatchDone;
}

static twin_dispatch_result_t _loop_cb(twin_widget_t *widget,
                                       twin_event_t *event,
                                       void *closure)
{
    (void)widget;
    if (event->kind != TwinEventButtonSignalUp)
        return TwinDispatchContinue;

    lottie_app_t *app = closure;
    mado_lottie_set_loop(app->lottie, !mado_lottie_is_looping(app->lottie));
    update_status(app);
    return TwinDispatchDone;
}

/*
 * Public API
 */

void apps_lottie_start(twin_screen_t *screen,
                       const char *name,
                       const char *path,
                       int x,
                       int y)
{
    twin_pixmap_t *pix = twin_pixmap_from_file(path, TWIN_ARGB32);
    if (!pix) {
        return;
    }

    if (!pix->animation || !twin_animation_is_lottie(pix->animation)) {
        twin_pixmap_destroy(pix);
        return;
    }

    mado_lottie_image_t *lottie = twin_animation_get_lottie(pix->animation);
    twin_animation_t *anim = pix->animation;

    int win_w = anim->width + 10;
    int win_h = anim->height + 30;

    twin_toplevel_t *top = twin_toplevel_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, win_w, win_h, name);
    if (!top) {
        twin_pixmap_destroy(pix);
        return;
    }

    twin_custom_widget_t *display = twin_custom_widget_create(
        &top->box, 0xffe0e0e0, anim->width, anim->height, 1, 10,
        _lottie_dispatch, sizeof(lottie_app_t));

    if (!display) {
        twin_pixmap_destroy(pix);
        return;
    }

    lottie_app_t *app = (lottie_app_t *)twin_custom_widget_data(display);
    app->lottie = lottie;
    app->pix = pix;
    app->display = display;

    /* Controls: Play | Loop | Status */
    twin_box_t *bar = twin_box_create(&top->box, TwinBoxHorz);

    twin_button_t *play = twin_button_create(bar, "Play", 0xff000000,
                                             BUTTON_SIZE, TwinStyleBold);
    twin_widget_set_callback(&play->label.widget, _play_cb, app);

    twin_button_t *loop = twin_button_create(bar, "Loop", 0xff000000,
                                             BUTTON_SIZE, TwinStyleBold);
    twin_widget_set_callback(&loop->label.widget, _loop_cb, app);

    app->status = twin_label_create(bar, "Playing | Loop", 0xff000000,
                                    BUTTON_SIZE, TwinStyleBold);

    app->timeout = twin_set_timeout(_lottie_timeout,
                                    mado_lottie_get_frame_delay(lottie), app);

    twin_toplevel_show(top);
}