/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Call Log: recent calls, newest first.  Tapping a row calls back.
 *
 * Rows are painted straight from the const data table at their scrolled
 * position; scrolling blits and repaints only the exposed band.
 */

#include <stdio.h>
#include <string.h>

#include "data.h"
#include "phone.h"

#define ROW_H PHONE_LIST_ROW_H
#define LIST_TOP PHONE_HEADER_H
#define ICON_CX 20

typedef struct {
    ui_view_t view;
    ui_scroll_t scroll;
    int pressed; /* row index, -1 when none */
    char missed[16];
} calllog_t;

static void calllog_layout(ui_view_t *v)
{
    calllog_t *l = (calllog_t *) v;
    ui_scroll_setup(&l->scroll,
                    ui_rect(0, LIST_TOP, PHONE_W, ui_view_height(v) - LIST_TOP),
                    PHONE_CALLLOG_COUNT * ROW_H);
}

static ui_rect_t row_rect(calllog_t *l, int i)
{
    return ui_rect(0, LIST_TOP + i * ROW_H - l->scroll.offset, PHONE_W, ROW_H);
}

/*
 * Direction glyph: an arrow into the call (incoming, missed) or out of it.
 * Shape tells in from out even where the theme makes their colors equal.
 */
static void paint_direction(twin_pixmap_t *px,
                            twin_coord_t cx,
                            twin_coord_t cy,
                            phone_call_dir_t dir,
                            twin_argb32_t color)
{
    /* Tail top-right, tip bottom-left; outgoing swaps them */
    twin_coord_t tx = cx - 5, ty = cy + 5, fx = cx + 5, fy = cy - 5;
    int s = 1;
    if (dir == PHONE_CALL_OUT) {
        tx = cx + 5, ty = cy - 5, fx = cx - 5, fy = cy + 5;
        s = -1;
    }
    ui_line(px, fx, fy, tx, ty, 2, color);
    ui_line(px, tx, ty, tx + 7 * s, ty, 2, color);
    ui_line(px, tx, ty, tx, ty - 7 * s, 2, color);
}

static void paint_row(twin_pixmap_t *px,
                      calllog_t *l,
                      int i,
                      const phone_call_t *call)
{
    const phone_theme_t *th = phone_theme();
    ui_rect_t r = row_rect(l, i);
    bool missed = call->dir == PHONE_CALL_MISSED;
    twin_argb32_t accent = missed                       ? th->missed_red
                           : call->dir == PHONE_CALL_IN ? th->call_green
                                                        : th->primary;

    ui_fill(px, r,
            i == l->pressed ? ui_mix(th->card, th->primary, 70) : th->card);
    twin_coord_t cy = (r.top + r.bottom) / 2;
    ui_circle(px, ICON_CX, cy, 14, ui_mix(th->card, accent, 40));
    paint_direction(px, ICON_CX, cy, call->dir, accent);

    /* "Today, 14:22": day over time in a right-hand column */
    char day[24];
    const char *comma = strchr(call->time, ',');
    const char *clock = comma ? comma + 1 : "";
    size_t n = comma ? (size_t) (comma - call->time) : strlen(call->time);
    if (n >= sizeof(day))
        n = sizeof(day) - 1;
    memcpy(day, call->time, n);
    day[n] = 0;
    while (*clock == ' ')
        clock++;
    twin_coord_t right = r.right - 10;
    twin_coord_t dw = ui_text_width(FONT_SM, day);
    ui_text(px, FONT_SM, right, r.top + 17, UI_RIGHT, th->text_sec, day);
    ui_text(px, FONT_SM, right, r.top + 32, UI_RIGHT, th->text_sec, clock);

    twin_coord_t x = ICON_CX + 18;
    ui_rect_t text = ui_rect(x - 4, r.top, right - dw - 6 - x, ROW_H);
    ui_text_fit(px, FONT_NORMAL,
                ui_rect(text.left, r.top + 3, ui_w(text), ROW_H / 2 - 1),
                UI_LEFT, missed ? th->missed_red : th->text, call->name);
    ui_text_fit(
        px, FONT_SM,
        ui_rect(text.left, r.top + ROW_H / 2 - 1, ui_w(text), ROW_H / 2 - 2),
        UI_LEFT, th->text_sec, call->phone);
    ui_separator(px, ui_rect(x, r.top, PHONE_W - x, ROW_H));
}

static void calllog_paint(ui_view_t *v, twin_pixmap_t *px)
{
    calllog_t *l = (calllog_t *) v;
    const phone_theme_t *th = phone_theme();
    const phone_call_t *log = phone_calllog();
    ui_rect_t clip = ui_view_clip(v);

    /* Rows first: the header paints over any row scrolled beneath it */
    for (int i = 0; i < PHONE_CALLLOG_COUNT; i++) {
        ui_rect_t r = row_rect(l, i);
        if (r.bottom <= LIST_TOP)
            continue;
        if (r.top >= ui_view_height(v))
            break;
        if (ui_intersects(r, clip))
            paint_row(px, l, i, &log[i]);
    }
    ui_rect_t below =
        ui_rect(0, LIST_TOP + PHONE_CALLLOG_COUNT * ROW_H - l->scroll.offset,
                PHONE_W, ui_view_height(v));
    if (ui_intersects(below, clip))
        ui_fill(px, below, th->bg);
    ui_scroll_bar(px, &l->scroll);

    ui_rect_t head = ui_rect(0, 0, PHONE_W, LIST_TOP);
    if (ui_intersects(head, clip))
        ui_header(px, head, NULL, v->title, l->missed[0] ? l->missed : NULL);
}

static int row_at(calllog_t *l, twin_coord_t x, twin_coord_t y)
{
    if (!ui_hit(l->scroll.area, x, y))
        return -1;
    int i = (y - LIST_TOP + l->scroll.offset) / ROW_H;
    return i >= 0 && i < PHONE_CALLLOG_COUNT ? i : -1;
}

static void invalidate_row(calllog_t *l, int i)
{
    if (i >= 0)
        ui_view_invalidate(&l->view, row_rect(l, i));
}

static ui_rect_t view_row(ui_view_t *v, int i)
{
    return row_rect((calllog_t *) v, i);
}

static void set_pressed(calllog_t *l, int i)
{
    ui_view_set_pressed(&l->view, &l->pressed, i, view_row);
}

static bool calllog_event(ui_view_t *v, const ui_event_t *e)
{
    calllog_t *l = (calllog_t *) v;
    int was = l->pressed;
    /*
     * A drag lifts the highlight first.  Its repaint may still be pending
     * when the blit moves the row, so repaint the row where it lands too.
     */
    if (e->kind == UI_DRAG || e->kind == UI_RELEASE)
        set_pressed(l, -1);
    if (ui_scroll_event(v, &l->scroll, e)) {
        invalidate_row(l, was);
        return true;
    }
    switch (e->kind) {
    case UI_PRESS:
    case UI_MOVE:
        set_pressed(l, row_at(l, e->x, e->y));
        return true;
    case UI_TAP: {
        int i = row_at(l, e->x, e->y);
        set_pressed(l, -1);
        if (i >= 0) {
            const phone_call_t *call = &phone_calllog()[i];
            app_dialer_call(call->name, call->phone);
        }
        return true;
    }
    default:
        set_pressed(l, -1);
        return false;
    }
}

void app_calllog_open(void)
{
    calllog_t *l =
        ui_view_new(sizeof(*l), "Call Log", calllog_paint, calllog_event, NULL);
    if (!l)
        return;
    l->pressed = -1;
    ui_view_set_layout(&l->view, calllog_layout);
    int missed = 0;
    for (int i = 0; i < PHONE_CALLLOG_COUNT; i++)
        missed += phone_calllog()[i].dir == PHONE_CALL_MISSED;
    if (missed)
        snprintf(l->missed, sizeof(l->missed), "%d missed", missed);
    ui_view_show(&l->view);
}

/* Test hook: scroll offset of the call log on top, -1 otherwise */
int app_calllog_test_offset(void)
{
    ui_view_t *v = phone_top();
    return v && v->paint == calllog_paint ? ((calllog_t *) v)->scroll.offset
                                          : -1;
}
