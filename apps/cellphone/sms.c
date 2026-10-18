/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Messages: conversation list and chat.
 *
 * The chat lays its bubbles out once per layout change (open, keyboard
 * shown or hidden, message sent) into per-message offsets in the view
 * struct; painting walks that table and skips every bubble outside the
 * clip, so a scroll step wraps and draws only the bubbles crossing the
 * exposed band.  Typing repaints the text field, and the send button only
 * when its enabled state flips.
 */

#include <string.h>

#include "data.h"
#include "phone.h"

#define LIST_ROW_H 52
#define LIST_AVATAR_R 17

#define COMPOSER_H 36
#define FIELD_H 28
#define SEND_W 40

#define BUBBLE_MAX_W (PHONE_W * 70 / 100)
#define BUBBLE_PAD_X 10
#define BUBBLE_PAD_Y 6
#define BUBBLE_R 12
#define BUBBLE_TAIL 14 /* corner square with a tighter radius */
#define LINE_H 17
#define TIME_H 15 /* timestamp line under a bubble */
#define MSG_GAP 5
#define SIDE 8
#define CHAT_PAD 8

/* Messages sent in this session are kept per open chat (stock is const) */
#define MAX_EXTRA 8
#define MAX_MSGS 32

/*
 * Conversation list
 */
typedef struct {
    ui_view_t view;
    ui_scroll_t scroll;
    int pressed; /* thread under the finger, -1 when none */
} sms_list_t;

/*
 * Chat
 */
typedef struct {
    twin_coord_t y; /* bubble top in content coordinates */
    twin_coord_t w, h;
} bubble_t;

typedef struct {
    char text[sizeof(((ui_field_t *) 0)->text)];
    char time[8];
} sent_msg_t;

typedef struct {
    ui_view_t view;
    const phone_sms_thread_t *thread;
    int stock;   /* stock messages shown (clamped to fit the table) */
    int n_extra; /* messages sent since the chat opened */
    sent_msg_t extra[MAX_EXTRA];
    bubble_t bubble[MAX_MSGS];
    twin_coord_t content_h;
    ui_scroll_t scroll;
    ui_field_t field;
    ui_kbd_t kbd;
    bool send_pressed;
    bool send_enabled; /* last painted state, to repaint only on change */
    /* Smooth scroll to the newest message after a send */
    twin_coord_t glide_from, glide_to;
    bool gliding;
} sms_chat_t;

/*
 * Chat geometry
 */
static int msg_count(const sms_chat_t *c)
{
    return c->stock + c->n_extra;
}

static const char *msg_text(const sms_chat_t *c, int i)
{
    return i < c->stock ? c->thread->messages[i].text
                        : c->extra[i - c->stock].text;
}

static const char *msg_time(const sms_chat_t *c, int i)
{
    return i < c->stock ? c->thread->messages[i].time
                        : c->extra[i - c->stock].time;
}

static bool msg_sent(const sms_chat_t *c, int i)
{
    return i < c->stock ? c->thread->messages[i].is_sent != 0 : true;
}

static twin_coord_t composer_top(const sms_chat_t *c)
{
    return ui_view_height(&c->view) - COMPOSER_H -
           (c->kbd.visible ? UI_KBD_H : 0);
}

static ui_rect_t composer_rect(const sms_chat_t *c)
{
    return ui_rect(0, composer_top(c), PHONE_W, COMPOSER_H);
}

static ui_rect_t field_rect(const sms_chat_t *c)
{
    twin_coord_t top = composer_top(c) + (COMPOSER_H - FIELD_H) / 2;
    return ui_rect(SIDE - 2, top, PHONE_W - 3 * (SIDE - 2) - SEND_W, FIELD_H);
}

static ui_rect_t send_rect(const sms_chat_t *c)
{
    twin_coord_t top = composer_top(c) + (COMPOSER_H - FIELD_H) / 2;
    return ui_rect(PHONE_W - (SIDE - 2) - SEND_W, top, SEND_W, FIELD_H);
}

static ui_rect_t kbd_rect(const sms_chat_t *c)
{
    return ui_kbd_rect(ui_view_height(&c->view));
}

static ui_rect_t chat_area(const sms_chat_t *c)
{
    return ui_rect(0, PHONE_HEADER_H, PHONE_W,
                   composer_top(c) - PHONE_HEADER_H);
}

/* Bubble rectangle of message @i in view coordinates at the current scroll */
static ui_rect_t bubble_rect(const sms_chat_t *c, int i)
{
    const bubble_t *b = &c->bubble[i];
    twin_coord_t top = c->scroll.area.top + b->y - c->scroll.offset;
    twin_coord_t left = msg_sent(c, i) ? PHONE_W - SIDE - b->w : SIDE;
    return ui_rect(left, top, b->w, b->h);
}

/* Measure every bubble; run on open and whenever a message is added */
static void layout_messages(sms_chat_t *c)
{
    twin_coord_t y = CHAT_PAD;
    for (int i = 0; i < msg_count(c); i++) {
        twin_coord_t widest = 0;
        int lines = ui_text_wrap(NULL, FONT_NORMAL, 0, 0,
                                 BUBBLE_MAX_W - 2 * BUBBLE_PAD_X, LINE_H, 0,
                                 msg_text(c, i), &widest);
        if (lines < 1)
            lines = 1;
        bubble_t *b = &c->bubble[i];
        b->y = y;
        b->w = widest + 2 * BUBBLE_PAD_X;
        if (b->w < 2 * BUBBLE_R + 8)
            b->w = 2 * BUBBLE_R + 8;
        b->h = lines * LINE_H + 2 * BUBBLE_PAD_Y;
        y += b->h + TIME_H + MSG_GAP;
    }
    c->content_h = y + CHAT_PAD - MSG_GAP;
}

/* Fit the viewport between header and composer, keeping the offset */
static void fit_viewport(sms_chat_t *c)
{
    c->scroll.flinging = false;
    ui_scroll_setup(&c->scroll, chat_area(c), c->content_h);
}

static void chat_layout(ui_view_t *v)
{
    fit_viewport((sms_chat_t *) v);
}

/* Same, then bring the newest message into view */
static void layout_viewport(sms_chat_t *c)
{
    c->gliding = false;
    fit_viewport(c);
    c->scroll.offset = ui_scroll_max(&c->scroll);
}

/*
 * Chat painting
 */
static void paint_bubble(sms_chat_t *c, twin_pixmap_t *px, int i)
{
    const phone_theme_t *th = phone_theme();
    ui_rect_t r = bubble_rect(c, i);
    bool sent = msg_sent(c, i);
    twin_argb32_t fill = sent ? th->sms_sent : th->sms_recv;

    ui_rrect(px, r, BUBBLE_R, fill);
    /* Tighter corner on the speaker's side, pointing at the sender */
    ui_rect_t tail = ui_rect(sent ? r.right - BUBBLE_TAIL : r.left,
                             r.bottom - BUBBLE_TAIL, BUBBLE_TAIL, BUBBLE_TAIL);
    ui_rrect(px, tail, 3, fill);

    twin_coord_t base = r.top + BUBBLE_PAD_Y + (LINE_H + 10) / 2;
    ui_text_wrap(px, FONT_NORMAL, r.left + BUBBLE_PAD_X, base,
                 BUBBLE_MAX_W - 2 * BUBBLE_PAD_X, LINE_H, th->text,
                 msg_text(c, i), NULL);

    twin_coord_t ty = r.bottom + TIME_H - 3;
    if (sent)
        ui_text(px, FONT_SM, r.right - 4, ty, UI_RIGHT, th->text_sec,
                msg_time(c, i));
    else
        ui_text(px, FONT_SM, r.left + 4, ty, UI_LEFT, th->text_sec,
                msg_time(c, i));
}

static void paint_messages(sms_chat_t *c, twin_pixmap_t *px, ui_rect_t clip)
{
    ui_rect_t area = c->scroll.area;
    /* Bubbles may straddle the viewport edge: keep them off header/composer */
    twin_rect_t saved = ui_clip_push(px, area);
    ui_fill(px, area, phone_theme()->bg);

    twin_coord_t lo = clip.top > area.top ? clip.top : area.top;
    twin_coord_t hi = clip.bottom < area.bottom ? clip.bottom : area.bottom;
    for (int i = 0; i < msg_count(c); i++) {
        ui_rect_t r = bubble_rect(c, i);
        if (r.bottom + TIME_H <= lo)
            continue;
        if (r.top >= hi)
            break; /* bubbles are laid out top to bottom */
        paint_bubble(c, px, i);
    }
    ui_scroll_bar(px, &c->scroll);
    twin_pixmap_restore_clip(px, saved);
}

static void paint_header(sms_chat_t *c, twin_pixmap_t *px)
{
    const phone_theme_t *th = phone_theme();
    const char *name = c->thread->contact_name;
    ui_rect_t r = ui_rect(0, 0, PHONE_W, PHONE_HEADER_H);
    ui_header(px, r, "", NULL);
    ui_avatar(px, 20, PHONE_HEADER_H / 2, 10, name[0], ui_avatar_color(name));
    ui_text_fit(px, FONT_HEADING, ui_rect(32, 0, PHONE_W - 40, PHONE_HEADER_H),
                UI_LEFT, th->text, name);
}

/*
 * ui_field() ellipsizes the end of long text; while composing the end is
 * what matters, so hand it the longest tail that fits instead.
 */
static void paint_field(sms_chat_t *c, twin_pixmap_t *px)
{
    ui_rect_t r = field_rect(c);
    twin_coord_t room = ui_w(r) - 20;
    const char *s = c->field.text;
    while (*s && ui_text_width(FONT_NORMAL, s) > room)
        s++;
    if (s == c->field.text) {
        ui_field(px, r, &c->field, "Type a message...");
        return;
    }
    ui_field_t tail = c->field;
    ui_field_set(&tail, s);
    ui_field(px, r, &tail, NULL);
}

static void paint_send(sms_chat_t *c, twin_pixmap_t *px)
{
    const phone_theme_t *th = phone_theme();
    ui_rect_t r = send_rect(c);
    twin_argb32_t bg, fg;
    if (!c->field.len) {
        /* Disabled: fade toward the composer instead of going translucent */
        bg = ui_mix(th->navbar, th->primary, 80);
        fg = ui_mix(bg, th->on_primary, 120);
    } else {
        bg = c->send_pressed ? th->primary_dk : th->primary;
        fg = th->on_primary;
    }
    ui_rrect(px, r, FIELD_H / 2, bg);
    ui_icon(px, TWIN_SYM_SEND, FONT_NORMAL, (r.left + r.right) / 2 - 1,
            (r.top + r.bottom) / 2, fg);
    c->send_enabled = c->field.len > 0;
}

static void paint_composer(sms_chat_t *c, twin_pixmap_t *px)
{
    const phone_theme_t *th = phone_theme();
    ui_rect_t r = composer_rect(c);
    ui_fill(px, r, ui_mix(th->navbar, th->bg, 110));
    ui_fill(px, ui_rect(0, r.top, PHONE_W, 1), th->separator);
    paint_field(c, px);
    paint_send(c, px);
}

static void chat_paint(ui_view_t *v, twin_pixmap_t *px)
{
    sms_chat_t *c = (sms_chat_t *) v;
    ui_rect_t clip = ui_view_clip(v);

    if (ui_intersects(clip, c->scroll.area))
        paint_messages(c, px, clip);
    if (ui_intersects(clip, ui_rect(0, 0, PHONE_W, PHONE_HEADER_H)))
        paint_header(c, px);
    if (ui_intersects(clip, composer_rect(c)))
        paint_composer(c, px);
    if (c->kbd.visible && ui_intersects(clip, kbd_rect(c)))
        ui_kbd_draw(px, &c->kbd, kbd_rect(c));
}

/*
 * Chat behavior
 */
static void field_changed(sms_chat_t *c)
{
    ui_view_invalidate(&c->view, field_rect(c));
    if (c->send_enabled != (c->field.len > 0))
        ui_view_invalidate(&c->view, send_rect(c));
}

static void set_keyboard(sms_chat_t *c, bool visible)
{
    if (c->kbd.visible == visible)
        return;
    ui_kbd_init(&c->kbd);
    c->kbd.visible = visible;
    c->field.focused = visible;
    c->send_pressed = false;
    layout_viewport(c);
    ui_view_invalidate_all(&c->view);
}

static bool glide_step(ui_view_t *v, twin_fixed_t value)
{
    sms_chat_t *c = (sms_chat_t *) v;
    if (!c->gliding)
        return false;
    ui_scroll_to(v, &c->scroll, ui_lerp(c->glide_from, c->glide_to, value));
    if (value >= TWIN_FIXED_ONE)
        c->gliding = false;
    return true;
}

static void send_message(sms_chat_t *c)
{
    if (!c->field.len)
        return;
    if (c->n_extra == MAX_EXTRA) {
        /* Full: forget the oldest message sent in this session */
        memmove(&c->extra[0], &c->extra[1],
                sizeof(c->extra[0]) * (MAX_EXTRA - 1));
        c->n_extra--;
    }
    sent_msg_t *m = &c->extra[c->n_extra++];
    memcpy(m->text, c->field.text, sizeof(m->text));
    phone_format_time(m->time, sizeof(m->time));
    ui_field_set(&c->field, "");
    c->send_pressed = false;

    /* Keep the current offset, then glide down to reveal the new bubble */
    twin_coord_t from = c->scroll.offset;
    layout_messages(c);
    fit_viewport(c);
    c->scroll.offset = from;
    ui_view_invalidate_all(&c->view);
    c->glide_from = from;
    c->glide_to = ui_scroll_max(&c->scroll);
    c->gliding = c->glide_to != from &&
                 ui_view_tween(&c->view, 220, TwinEaseOutCubic, glide_step);
    if (!c->gliding)
        c->scroll.offset = c->glide_to;
}

static void set_send_pressed(sms_chat_t *c, bool pressed)
{
    if (c->send_pressed == pressed)
        return;
    c->send_pressed = pressed;
    ui_view_invalidate(&c->view, send_rect(c));
}

/* Route a press-tracking event to the keyboard; repaint on visual change */
static void kbd_feed(sms_chat_t *c, const ui_event_t *e)
{
    int key = ui_kbd_event(&c->kbd, kbd_rect(c), e);
    if (!ui_empty(c->kbd.dirty))
        ui_view_invalidate(&c->view, c->kbd.dirty);
    if (key == UI_KEY_ENTER)
        send_message(c);
    else if (key != UI_KEY_NONE && ui_field_key(&c->field, key))
        field_changed(c);
}

static bool in_kbd(const sms_chat_t *c, twin_coord_t x, twin_coord_t y)
{
    return c->kbd.visible && ui_hit(kbd_rect(c), x, y);
}

static bool chat_event(ui_view_t *v, const ui_event_t *e)
{
    sms_chat_t *c = (sms_chat_t *) v;

    if (e->kind == UI_PRESS)
        c->gliding = false; /* the finger takes over from the glide */
    if (ui_scroll_event(v, &c->scroll, e)) {
        set_send_pressed(c, false);
        return true;
    }

    switch (e->kind) {
    case UI_PRESS:
        if (in_kbd(c, e->x, e->y))
            kbd_feed(c, e);
        else if (c->field.len && ui_hit(send_rect(c), e->x, e->y))
            set_send_pressed(c, true);
        return true;
    case UI_MOVE:
    case UI_DRAG:
        if (in_kbd(c, e->x0, e->y0))
            kbd_feed(c, e);
        else
            set_send_pressed(
                c, c->send_pressed && ui_hit(send_rect(c), e->x, e->y));
        return true;
    case UI_TAP:
        if (in_kbd(c, e->x0, e->y0)) {
            kbd_feed(c, e);
        } else if (ui_hit(send_rect(c), e->x, e->y)) {
            /* Disabled while empty: send_message() ignores an empty field */
            if (c->send_pressed)
                send_message(c);
        } else if (ui_hit(field_rect(c), e->x, e->y)) {
            set_keyboard(c, true);
        } else if (e->y < composer_top(c)) {
            set_keyboard(c, false); /* tap outside the composer */
        }
        set_send_pressed(c, false);
        return true;
    case UI_RELEASE:
    case UI_CANCEL:
        if (c->kbd.visible)
            kbd_feed(c, e);
        set_send_pressed(c, false);
        return true;
    case UI_KEY: {
        if (e->ucs4 == UI_KEY_ENTER) {
            send_message(c);
            return true;
        }
        if (ui_field_key(&c->field, (int) e->ucs4)) {
            field_changed(c);
            return true;
        }
        return false;
    }
    default:
        return false;
    }
}

static void chat_open(const phone_sms_thread_t *thread, int app)
{
    sms_chat_t *c = ui_view_new(sizeof(*c), thread->contact_name, chat_paint,
                                chat_event, NULL);
    if (!c)
        return;
    c->view.app = app;
    c->thread = thread;
    c->stock = (int) thread->msg_count;
    if (c->stock > MAX_MSGS - MAX_EXTRA)
        c->stock = MAX_MSGS - MAX_EXTRA;
    ui_kbd_init(&c->kbd);
    layout_messages(c);
    layout_viewport(c);
    ui_view_set_layout(&c->view, chat_layout);
    ui_view_show(&c->view);
}

/*
 * Conversation list
 */
static void list_layout(ui_view_t *v)
{
    sms_list_t *l = (sms_list_t *) v;
    ui_scroll_setup(
        &l->scroll,
        ui_rect(0, PHONE_HEADER_H, PHONE_W, ui_view_height(v) - PHONE_HEADER_H),
        PHONE_SMS_THREADS * LIST_ROW_H);
}

static ui_rect_t row_rect(const sms_list_t *l, int i)
{
    return ui_rect(0, PHONE_HEADER_H + i * LIST_ROW_H - l->scroll.offset,
                   PHONE_W, LIST_ROW_H);
}

static void paint_row(const sms_list_t *l, twin_pixmap_t *px, int i)
{
    const phone_theme_t *th = phone_theme();
    const phone_sms_thread_t *t = &phone_sms_threads()[i];
    ui_rect_t r = row_rect(l, i);
    twin_coord_t cy = (r.top + r.bottom) / 2;
    twin_coord_t x = 12 + 2 * LIST_AVATAR_R + 10;

    ui_fill(px, r,
            i == l->pressed ? ui_mix(th->card, th->primary, 70) : th->card);
    ui_avatar(px, 12 + LIST_AVATAR_R, cy, LIST_AVATAR_R, t->contact_name[0],
              ui_avatar_color(t->contact_name));

    twin_coord_t tw = ui_text_width(FONT_SM, t->timestamp);
    ui_text(px, FONT_SM, PHONE_W - 10, r.top + 21, UI_RIGHT, th->text_sec,
            t->timestamp);
    /* ui_text_fit pads 4 px on each side; shift the box to align at x */
    ui_text_fit(px, FONT_NORMAL,
                ui_rect(x - 4, r.top + 8, PHONE_W - 10 - tw - 4 - (x - 4), 18),
                UI_LEFT, th->text, t->contact_name);
    ui_text_fit(px, FONT_SM, ui_rect(x - 4, r.top + 27, PHONE_W - 6 - x, 16),
                UI_LEFT, th->text_sec, t->preview);
    ui_separator(px, ui_rect(x, r.top, PHONE_W - x, LIST_ROW_H));
}

static void list_paint(ui_view_t *v, twin_pixmap_t *px)
{
    sms_list_t *l = (sms_list_t *) v;
    ui_rect_t clip = ui_view_clip(v);
    ui_rect_t area = l->scroll.area;

    if (ui_intersects(clip, area)) {
        twin_rect_t saved = ui_clip_push(px, area);
        ui_fill(px, area, phone_theme()->bg);
        for (int i = 0; i < PHONE_SMS_THREADS; i++)
            if (ui_intersects(row_rect(l, i), clip))
                paint_row(l, px, i);
        ui_scroll_bar(px, &l->scroll);
        twin_pixmap_restore_clip(px, saved);
    }
    if (ui_intersects(clip, ui_rect(0, 0, PHONE_W, PHONE_HEADER_H)))
        ui_header(px, ui_rect(0, 0, PHONE_W, PHONE_HEADER_H), "Messages", NULL);
}

static int row_at(const sms_list_t *l, twin_coord_t x, twin_coord_t y)
{
    if (!ui_hit(l->scroll.area, x, y))
        return -1;
    for (int i = 0; i < PHONE_SMS_THREADS; i++)
        if (ui_hit(row_rect(l, i), x, y))
            return i;
    return -1;
}

static ui_rect_t list_row(ui_view_t *v, int i)
{
    return row_rect((sms_list_t *) v, i);
}

static void list_set_pressed(sms_list_t *l, int i)
{
    ui_view_set_pressed(&l->view, &l->pressed, i, list_row);
}

static bool list_event(ui_view_t *v, const ui_event_t *e)
{
    sms_list_t *l = (sms_list_t *) v;
    if (ui_scroll_event(v, &l->scroll, e)) {
        list_set_pressed(l, -1);
        return true;
    }
    switch (e->kind) {
    case UI_PRESS:
        list_set_pressed(l, row_at(l, e->x, e->y));
        return true;
    case UI_MOVE:
        if (row_at(l, e->x, e->y) != l->pressed)
            list_set_pressed(l, -1);
        return true;
    case UI_TAP: {
        int i = row_at(l, e->x, e->y);
        list_set_pressed(l, -1);
        if (i >= 0)
            chat_open(&phone_sms_threads()[i], v->app);
        return true;
    }
    case UI_DRAG:
    case UI_RELEASE:
    case UI_CANCEL:
        list_set_pressed(l, -1);
        return true;
    default:
        return false;
    }
}

void app_sms_open(void)
{
    sms_list_t *l =
        ui_view_new(sizeof(*l), "Messages", list_paint, list_event, NULL);
    if (!l)
        return;
    l->pressed = -1;
    ui_view_set_layout(&l->view, list_layout);
    ui_view_show(&l->view);
}

/*
 * Test hooks: state of the chat on top, or -1 / NULL when none
 */
static sms_chat_t *top_chat(void)
{
    ui_view_t *v = phone_top();
    return v && v->paint == chat_paint ? (sms_chat_t *) v : NULL;
}

int app_sms_test_offset(void)
{
    sms_chat_t *c = top_chat();
    return c ? c->scroll.offset : -1;
}

int app_sms_test_max_offset(void)
{
    sms_chat_t *c = top_chat();
    return c ? ui_scroll_max(&c->scroll) : -1;
}

int app_sms_test_messages(void)
{
    sms_chat_t *c = top_chat();
    return c ? msg_count(c) : -1;
}

int app_sms_test_keyboard(void)
{
    sms_chat_t *c = top_chat();
    return c ? c->kbd.visible : -1;
}

const char *app_sms_test_field(void)
{
    sms_chat_t *c = top_chat();
    return c ? c->field.text : NULL;
}

/* Text of message @i in the chat on top */
const char *app_sms_test_message(int i)
{
    sms_chat_t *c = top_chat();
    return c && i >= 0 && i < msg_count(c) ? msg_text(c, i) : NULL;
}
