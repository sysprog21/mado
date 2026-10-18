/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Lock screen: clock, date, slide to unlock.
 *
 * The hint text breathes on a timer that repaints only its own rectangle,
 * and the clock repaints once a minute; the rest of the screen is never
 * touched while the phone sits locked.
 */

#include "phone.h"

#define TRACK_X 20
#define TRACK_W (PHONE_W - 2 * TRACK_X)
#define TRACK_Y 284
#define TRACK_H 22
#define KNOB_R 11
#define HINT_Y 266
#define TIME_Y 74
#define DATE_Y 104

typedef struct {
    ui_view_t view;
    twin_coord_t knob; /* knob travel in px, 0 .. TRACK_W - 2 * KNOB_R */
    twin_coord_t from; /* knob position when a spring-back started */
    bool grabbed;
    int phase; /* breathing phase, 0..4095 */
    twin_a8_t hint_alpha;
    int minute;
} lock_t;

static twin_coord_t travel(void)
{
    return TRACK_W - 2 * KNOB_R - 4;
}

static ui_rect_t hint_rect(void)
{
    return ui_rect(40, HINT_Y - 14, PHONE_W - 80, 20);
}

static ui_rect_t track_rect(void)
{
    return ui_rect(TRACK_X, TRACK_Y, TRACK_W, TRACK_H);
}

static ui_rect_t clock_rect(void)
{
    return ui_rect(0, TIME_Y - 36, PHONE_W, DATE_Y - TIME_Y + 46);
}

static void lock_paint(ui_view_t *v, twin_pixmap_t *px)
{
    lock_t *l = (lock_t *) v;
    ui_vgrad(px, ui_view_bounds(v), UI_RGB(0x2a1414), UI_RGB(0x050505));

    char buf[48];
    phone_format_time(buf, sizeof(buf));
    ui_text(px, FONT_CLOCK, PHONE_W / 2, TIME_Y, UI_CENTER, UI_RGB(0xffffff),
            buf);
    phone_format_date(buf, sizeof(buf));
    ui_text(px, FONT_NORMAL, PHONE_W / 2, DATE_Y, UI_CENTER, UI_RGB(0xe6e6e6),
            buf);

    ui_text(px, FONT_SM, PHONE_W / 2, HINT_Y, UI_CENTER,
            ui_alpha(UI_RGB(0xd0d0d0), l->hint_alpha), "Slide to unlock");

    ui_rect_t t = track_rect();
    ui_rrect(px, t, TRACK_H / 2, ui_alpha(UI_RGB(0xffffff), 0x40));
    /* Lit trail behind the knob */
    ui_rrect(px, ui_rect(t.left, t.top, l->knob + 2 * KNOB_R + 4, TRACK_H),
             TRACK_H / 2, ui_alpha(UI_RGB(0x22c55e), 0x30));
    twin_coord_t kx = t.left + 2 + KNOB_R + l->knob;
    twin_coord_t ky = t.top + TRACK_H / 2;
    ui_circle(px, kx, ky + 1, KNOB_R + 1, 0x50000000);
    ui_circle(px, kx, ky, KNOB_R, UI_RGB(0xffffff));
    ui_icon(px, TWIN_SYM_RIGHT, FONT_SM, kx + 1, ky, UI_RGB(0x22c55e));
}

static twin_time_t lock_tick(ui_view_t *v)
{
    lock_t *l = (lock_t *) v;
    /* Breathe between 30% and 80% over ~1.8 s */
    l->phase = (l->phase + 4096 * 60 / 1800) & 4095;
    twin_fixed_t c = twin_cos((twin_angle_t) l->phase);
    int a =
        0x4d + (int) (((int64_t) (0xcc - 0x4d) * (TWIN_FIXED_ONE - c)) >> 17);
    if (a != l->hint_alpha) {
        l->hint_alpha = (twin_a8_t) a;
        ui_view_invalidate(v, hint_rect());
    }

    phone_clock_t clk;
    phone_clock(&clk);
    int minute = clk.hour * 60 + clk.min;
    if (minute != l->minute) {
        l->minute = minute;
        ui_view_invalidate(v, clock_rect());
    }
    return 60;
}

static void set_knob(lock_t *l, twin_coord_t knob)
{
    if (knob < 0)
        knob = 0;
    if (knob > travel())
        knob = travel();
    if (knob == l->knob)
        return;
    l->knob = knob;
    ui_view_invalidate(&l->view, ui_inset(track_rect(), -3));
}

static bool spring_back(ui_view_t *v, twin_fixed_t value)
{
    lock_t *l = (lock_t *) v;
    set_knob(l, l->from - twin_fixed_to_int(l->from * value));
    return true;
}

static bool lock_event(ui_view_t *v, const ui_event_t *e)
{
    lock_t *l = (lock_t *) v;
    ui_rect_t t = ui_inset(track_rect(), -10);
    switch (e->kind) {
    case UI_PRESS:
        l->grabbed = ui_hit(t, e->x, e->y);
        return true;
    case UI_MOVE:
    case UI_DRAG:
        if (l->grabbed)
            set_knob(l, e->dx);
        return true;
    case UI_RELEASE:
    case UI_TAP:
        if (!l->grabbed)
            return true;
        l->grabbed = false;
        if (l->knob * 10 >= travel() * 8) {
            /* Unlocked: the window manager slides the lock away */
            ui_view_close(v);
            return true;
        }
        l->from = l->knob;
        if (l->from)
            ui_view_tween(v, 180, TwinEaseOutCubic, spring_back);
        return true;
    default:
        return false;
    }
}

ui_view_t *lock_create(void)
{
    lock_t *l = ui_view_new_fullscreen(sizeof(*l), "Lock", lock_paint,
                                       lock_event, NULL);
    if (!l)
        return NULL;
    l->hint_alpha = 0x4d;
    l->minute = -1;
    ui_view_every(&l->view, 60, lock_tick);
    return &l->view;
}
