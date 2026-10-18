/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Contacts: A-Z list, contact card, and a "new contact" form.
 *
 * Each screen is its own view.  The list holds no copy of the data: it walks
 * phone_contact_at() while painting, so a contact added by the form shows up
 * in sorted position as soon as the list is revealed.  Scrolling blits the
 * rows already on screen and paints only the exposed band.
 */

#include <string.h>

#include "data.h"
#include "phone.h"

/*
 * List
 */
#define LIST_TOP PHONE_HEADER_H
#define PLUS_W 40
#define FRESH_MS 1200 /* how long a newly added row stays highlighted */

typedef struct {
    ui_view_t view;
    ui_scroll_t scroll;
    uint32_t count; /* contact count the layout was computed for */
    int pressed;    /* contact index under the finger, -1 when none */
    int fresh;      /* contact just added by the form, -1 when none */
    bool plus_down;
} contacts_list_t;

static ui_rect_t plus_rect(void)
{
    return ui_rect(PHONE_W - PLUS_W, 0, PLUS_W, PHONE_HEADER_H);
}

/* Content y of contact @idx's row (sections included) */
static twin_coord_t row_content_y(uint32_t idx)
{
    twin_coord_t y = 0;
    char prev = 0;
    for (uint32_t i = 0; i <= idx; i++) {
        const phone_contact_t *c = phone_contact_at(i);
        if (!c)
            break;
        if (c->initial != prev) {
            y += PHONE_SECTION_H;
            prev = c->initial;
        }
        if (i < idx)
            y += PHONE_LIST_ROW_H;
    }
    return y;
}

static ui_rect_t row_rect(contacts_list_t *l, int idx)
{
    return ui_rect(0,
                   LIST_TOP + row_content_y((uint32_t) idx) - l->scroll.offset,
                   PHONE_W, PHONE_LIST_ROW_H);
}

static void list_layout(ui_view_t *v)
{
    contacts_list_t *l = (contacts_list_t *) v;
    l->count = phone_contact_count();
    twin_coord_t h =
        l->count ? row_content_y(l->count - 1) + PHONE_LIST_ROW_H : 0;
    ui_scroll_setup(&l->scroll,
                    ui_rect(0, LIST_TOP, PHONE_W, ui_view_height(v) - LIST_TOP),
                    h);
    if (l->pressed >= (int) l->count)
        l->pressed = -1;
    if (l->fresh >= (int) l->count)
        l->fresh = -1;
}

/* Contact under client y, or -1 for sections and empty space */
static int row_at(contacts_list_t *l, twin_coord_t y)
{
    if (!ui_hit(l->scroll.area, 0, y))
        return -1;
    twin_coord_t cy = y - LIST_TOP + l->scroll.offset, top = 0;
    char prev = 0;
    for (uint32_t i = 0; i < l->count; i++) {
        const phone_contact_t *c = phone_contact_at(i);
        if (c->initial != prev) {
            top += PHONE_SECTION_H;
            prev = c->initial;
        }
        if (cy < top)
            return -1;
        if (cy < top + PHONE_LIST_ROW_H)
            return (int) i;
        top += PHONE_LIST_ROW_H;
    }
    return -1;
}

static void list_paint(ui_view_t *v, twin_pixmap_t *px)
{
    contacts_list_t *l = (contacts_list_t *) v;
    const phone_theme_t *th = phone_theme();
    ui_rect_t clip = ui_view_clip(v);
    ui_rect_t area = l->scroll.area;

    /* Data changed under us (e.g. reset): relayout and repaint it all */
    if (l->count != phone_contact_count()) {
        list_layout(v);
        ui_view_invalidate_all(v);
    }

    if (ui_intersects(clip, area)) {
        twin_rect_t saved = ui_clip_push(px, area);
        ui_fill(px, area, th->bg);
        twin_coord_t y = area.top - l->scroll.offset;
        char prev = 0;
        for (uint32_t i = 0; i < l->count && y < clip.bottom; i++) {
            const phone_contact_t *c = phone_contact_at(i);
            if (c->initial != prev) {
                ui_rect_t r = ui_rect(0, y, PHONE_W, PHONE_SECTION_H);
                char label[2] = {c->initial, 0};
                if (ui_intersects(r, clip))
                    ui_section(px, r, label);
                y += PHONE_SECTION_H;
                prev = c->initial;
            }
            ui_rect_t r = ui_rect(0, y, PHONE_W, PHONE_LIST_ROW_H);
            if (ui_intersects(r, clip)) {
                char initial[2] = {c->name[0], 0};
                ui_list_row(px, r, (int) i == l->pressed || (int) i == l->fresh,
                            initial, ui_avatar_color(c->name), c->name,
                            c->phone, NULL);
            }
            y += PHONE_LIST_ROW_H;
        }
        ui_scroll_bar(px, &l->scroll);
        twin_pixmap_restore_clip(px, saved);
    }

    ui_rect_t hdr = ui_rect(0, 0, PHONE_W, PHONE_HEADER_H);
    if (ui_intersects(clip, hdr)) {
        ui_header(px, hdr, "Contacts", TWIN_SYM_PLUS);
        if (l->plus_down) {
            ui_rect_t p = plus_rect();
            ui_rrect(px, ui_inset(p, 4), 6, ui_alpha(th->primary, 0x40));
            ui_icon(px, TWIN_SYM_PLUS, FONT_HEADING, (p.left + p.right) / 2,
                    (p.top + p.bottom) / 2, th->primary_dk);
        }
    }
}

static ui_rect_t list_row(ui_view_t *v, int idx)
{
    return row_rect((contacts_list_t *) v, idx);
}

static void list_set_pressed(contacts_list_t *l, int idx)
{
    ui_view_set_pressed(&l->view, &l->pressed, idx, list_row);
}

static void list_set_plus(contacts_list_t *l, bool down)
{
    if (down == l->plus_down)
        return;
    l->plus_down = down;
    ui_view_invalidate(&l->view, plus_rect());
}

static void detail_open(int idx);
static void form_open(void);

static bool list_event(ui_view_t *v, const ui_event_t *e)
{
    contacts_list_t *l = (contacts_list_t *) v;

    /* A drag must not carry the pressed highlight along: window damage is
     * a single rectangle that does not follow the scroll blit, so clear
     * the highlight and flush it before any pixels move. */
    if ((e->kind == UI_DRAG || e->kind == UI_RELEASE) && l->pressed >= 0) {
        list_set_pressed(l, -1);
        twin_window_draw(v->window);
    }
    if (ui_scroll_event(v, &l->scroll, e))
        return true;

    switch (e->kind) {
    case UI_PRESS:
        list_set_plus(l, ui_hit(plus_rect(), e->x, e->y));
        list_set_pressed(l, row_at(l, e->y));
        return true;
    case UI_MOVE:
        list_set_plus(l, l->plus_down && ui_hit(plus_rect(), e->x, e->y));
        if (l->pressed >= 0 && row_at(l, e->y) != l->pressed)
            list_set_pressed(l, -1);
        return true;
    case UI_TAP: {
        bool plus = l->plus_down && ui_hit(plus_rect(), e->x, e->y);
        int idx = l->pressed >= 0 ? row_at(l, e->y) : -1;
        list_set_plus(l, false);
        list_set_pressed(l, -1);
        if (plus)
            form_open();
        else if (idx >= 0)
            detail_open(idx);
        return true;
    }
    default:
        list_set_plus(l, false);
        list_set_pressed(l, -1);
        return e->kind != UI_KEY;
    }
}

static twin_time_t fresh_done(ui_view_t *v)
{
    contacts_list_t *l = (contacts_list_t *) v;
    if (l->fresh >= 0) {
        ui_view_invalidate(v, row_rect(l, l->fresh));
        l->fresh = -1;
    }
    return -1;
}

/*
 * Called by the form while the list is covered: relayout for the new
 * contact, bring its row into view and flash it.  The list repaints in full
 * when the form pops, so moving the offset needs no blit.
 */
static void list_added(contacts_list_t *l, int idx)
{
    list_layout(&l->view);
    twin_coord_t top = row_content_y((uint32_t) idx);
    twin_coord_t h = ui_h(l->scroll.area);
    twin_coord_t off = l->scroll.offset;
    if (top - PHONE_SECTION_H < off)
        off = top - PHONE_SECTION_H;
    else if (top + PHONE_LIST_ROW_H > off + h)
        off = top + PHONE_LIST_ROW_H - h;
    if (off > ui_scroll_max(&l->scroll))
        off = ui_scroll_max(&l->scroll);
    l->scroll.offset = off < 0 ? 0 : off;
    l->scroll.flinging = false;
    l->fresh = idx;
    ui_view_every(&l->view, FRESH_MS, fresh_done);
    ui_view_invalidate_all(&l->view);
}

void app_contacts_open(void)
{
    contacts_list_t *l =
        ui_view_new(sizeof(*l), "Contacts", list_paint, list_event, NULL);
    if (!l)
        return;
    l->pressed = l->fresh = -1;
    ui_view_set_layout(&l->view, list_layout);
    ui_view_show(&l->view);
}

/*
 * Detail
 */
enum {
    DETAIL_CALL,
    DETAIL_MESSAGE,
    DETAIL_EMAIL,
    DETAIL_PHONE_ROW,
    DETAIL_N,
};

#define AVATAR_R 30
#define AVATAR_CY 44
#define ACTION_Y 108
#define ACTION_W 68
#define ACTION_H 48
#define CARD_Y 168
#define CARD_ROW_H 40

typedef struct {
    ui_view_t view;
    int idx;     /* contact index */
    int pressed; /* DETAIL_*, -1 when none */
} contact_detail_t;

static ui_rect_t detail_rect(int what)
{
    if (what == DETAIL_PHONE_ROW)
        return ui_rect(PHONE_PAD, CARD_Y, PHONE_W - 2 * PHONE_PAD, CARD_ROW_H);
    twin_coord_t gap = (PHONE_W - 3 * ACTION_W) / 4;
    return ui_rect(gap + what * (ACTION_W + gap), ACTION_Y, ACTION_W, ACTION_H);
}

static void action_button(twin_pixmap_t *px,
                          ui_rect_t r,
                          twin_argb32_t bg,
                          twin_argb32_t fg,
                          bool pressed,
                          const char *sym,
                          const char *label)
{
    if (pressed)
        bg = ui_darken(bg, 18);
    ui_rrect(px, r, 10, bg);
    twin_coord_t cx = (r.left + r.right) / 2;
    ui_icon(px, sym, FONT_HEADING, cx, r.top + 18, fg);
    ui_text(px, FONT_SM, cx, r.bottom - 8, UI_CENTER, fg, label);
}

static void info_row(twin_pixmap_t *px,
                     ui_rect_t r,
                     const char *label,
                     const char *value,
                     twin_argb32_t value_color)
{
    const phone_theme_t *th = phone_theme();
    ui_text(px, FONT_SM, r.left + 10, r.top + 15, UI_LEFT, th->text_sec, label);
    ui_text_fit(px, FONT_NORMAL,
                ui_rect(r.left + 6, r.top + 16, ui_w(r) - 40, ui_h(r) - 18),
                UI_LEFT, value_color, value);
}

static void detail_paint(ui_view_t *v, twin_pixmap_t *px)
{
    contact_detail_t *d = (contact_detail_t *) v;
    const phone_theme_t *th = phone_theme();
    const phone_contact_t *c = phone_contact_at((uint32_t) d->idx);

    ui_vgrad(px, ui_rect(0, 0, PHONE_W, ACTION_Y), th->card, th->bg);
    ui_fill(px, ui_rect(0, ACTION_Y, PHONE_W, ui_view_height(v) - ACTION_Y),
            th->bg);
    if (!c) {
        ui_text(px, FONT_NORMAL, PHONE_W / 2, 120, UI_CENTER, th->text_sec,
                "Contact not found");
        return;
    }

    ui_circle(px, PHONE_W / 2, AVATAR_CY + 2, AVATAR_R + 1,
              ui_alpha(th->text, 0x28));
    ui_avatar(px, PHONE_W / 2, AVATAR_CY, AVATAR_R, c->name[0],
              ui_avatar_color(c->name));
    ui_text_fit(px, FONT_HEADING, ui_rect(PHONE_PAD, 78, PHONE_W - 16, 24),
                UI_CENTER, th->text, c->name);

    action_button(px, detail_rect(DETAIL_CALL), th->call_green,
                  UI_RGB(0xffffff), d->pressed == DETAIL_CALL, TWIN_SYM_PHONE,
                  "Call");
    action_button(px, detail_rect(DETAIL_MESSAGE), th->primary, th->on_primary,
                  d->pressed == DETAIL_MESSAGE, TWIN_SYM_MESSAGE, "Message");
    /* No mail client: drawn for completeness, but visibly inactive */
    action_button(px, detail_rect(DETAIL_EMAIL), th->card,
                  ui_mix(th->card, th->text_sec, 160), false, TWIN_SYM_MAIL,
                  "Email");

    ui_rect_t card =
        ui_rect(PHONE_PAD, CARD_Y, PHONE_W - 2 * PHONE_PAD, 2 * CARD_ROW_H);
    ui_rrect(px, card, 10, th->card);
    ui_rect_t phone_row = detail_rect(DETAIL_PHONE_ROW);
    if (d->pressed == DETAIL_PHONE_ROW) {
        ui_rect_t hl = phone_row;
        hl.bottom += 6; /* square off the lower corners under row 2 */
        ui_rrect(px, hl, 10, ui_mix(th->card, th->primary, 70));
        ui_fill(px, ui_rect(card.left, phone_row.bottom, ui_w(card), 6),
                th->card);
    }
    info_row(px, phone_row, "mobile", c->phone, th->text);
    ui_icon(px, TWIN_SYM_PHONE, FONT_NORMAL, phone_row.right - 18,
            (phone_row.top + phone_row.bottom) / 2, th->call_green);
    ui_fill(px,
            ui_rect(card.left + 10, phone_row.bottom - 1, ui_w(card) - 20, 1),
            th->separator);
    ui_rect_t mail_row =
        ui_rect(card.left, phone_row.bottom, ui_w(card), CARD_ROW_H);
    info_row(px, mail_row, "email", c->email[0] ? c->email : "-", th->text);
}

static int detail_hit(twin_coord_t x, twin_coord_t y)
{
    for (int i = 0; i < DETAIL_N; i++)
        if (i != DETAIL_EMAIL && ui_hit(detail_rect(i), x, y))
            return i;
    return -1;
}

static ui_rect_t detail_item(ui_view_t *v, int what)
{
    (void) v;
    return detail_rect(what);
}

static void detail_set_pressed(contact_detail_t *d, int what)
{
    ui_view_set_pressed(&d->view, &d->pressed, what, detail_item);
}

static bool detail_event(ui_view_t *v, const ui_event_t *e)
{
    contact_detail_t *d = (contact_detail_t *) v;
    switch (e->kind) {
    case UI_PRESS:
    case UI_MOVE:
        detail_set_pressed(d, detail_hit(e->x, e->y));
        return true;
    case UI_TAP: {
        int what = detail_hit(e->x, e->y);
        const phone_contact_t *c = phone_contact_at((uint32_t) d->idx);
        detail_set_pressed(d, -1);
        if (!c)
            return true;
        if (what == DETAIL_CALL || what == DETAIL_PHONE_ROW)
            app_dialer_call(c->name, c->phone);
        else if (what == DETAIL_MESSAGE)
            app_sms_open();
        return true;
    }
    case UI_KEY:
        return false;
    default:
        detail_set_pressed(d, -1);
        return true;
    }
}

static void detail_open(int idx)
{
    contact_detail_t *d =
        ui_view_new(sizeof(*d), "Contact", detail_paint, detail_event, NULL);
    if (!d)
        return;
    d->idx = idx;
    d->pressed = -1;
    ui_view_show(&d->view);
}

/*
 * New contact form
 */
#define FIELDS 3
#define FIELD_X PHONE_PAD
#define FIELD_W (PHONE_W - 2 * PHONE_PAD)
#define FIELD_H 30
#define FIELD_STEP 50
#define LABEL_H 15
#define FORM_TOP (PHONE_HEADER_H + 6)
#define ACTIONS_H 40

enum {
    FORM_CANCEL,
    FORM_SAVE,
};

/* Input limits match the backing buffers in data.c (32/24/40 bytes) */
static const struct {
    const char *label, *placeholder;
    int max;
} fields[FIELDS] = {
    {"Name", "Ashley Johnson", 31},
    {"Phone", "+1-555-0123", 23},
    {"Email", "ashley@example.com", 39},
};

typedef struct {
    ui_view_t view;
    ui_field_t field[FIELDS];
    int focus; /* focused field, -1 when none (keyboard hidden) */
    ui_kbd_t kbd;
    int pressed;       /* FORM_*, -1 when none */
    const char *error; /* validation message, NULL when none */
    bool name_error;   /* outline the name field */
} contact_form_t;

/*
 * With the keyboard up only a strip between the header and the action row
 * is left, so the form lifts to keep the focused field in that strip.
 */
static twin_coord_t form_lift(const contact_form_t *f)
{
    return f->focus >= 0 ? f->focus * FIELD_STEP : 0;
}

static twin_coord_t actions_top(const contact_form_t *f)
{
    twin_coord_t h = ui_view_height(&f->view);
    return f->kbd.visible ? h - UI_KBD_H - ACTIONS_H : h - ACTIONS_H - 4;
}

/* Label plus box of field @i, in client coordinates */
static ui_rect_t field_slot(const contact_form_t *f, int i)
{
    return ui_rect(0, FORM_TOP + i * FIELD_STEP - form_lift(f), PHONE_W,
                   LABEL_H + FIELD_H);
}

static ui_rect_t field_box(const contact_form_t *f, int i)
{
    ui_rect_t s = field_slot(f, i);
    return ui_rect(FIELD_X, s.top + LABEL_H, FIELD_W, FIELD_H);
}

static ui_rect_t form_strip(const contact_form_t *f)
{
    return ui_rect(0, PHONE_HEADER_H, PHONE_W, actions_top(f) - PHONE_HEADER_H);
}

static ui_rect_t error_rect(void)
{
    return ui_rect(0, FORM_TOP + FIELDS * FIELD_STEP - 2, PHONE_W, 18);
}

static ui_rect_t action_rect(const contact_form_t *f, int which)
{
    twin_coord_t w = (PHONE_W - 3 * PHONE_PAD) / 2;
    return ui_rect(PHONE_PAD + which * (w + PHONE_PAD), actions_top(f) + 4, w,
                   ACTIONS_H - 8);
}

static ui_rect_t kbd_rect(const contact_form_t *f)
{
    return ui_kbd_rect(ui_view_height(&f->view));
}

static void form_paint(ui_view_t *v, twin_pixmap_t *px)
{
    contact_form_t *f = (contact_form_t *) v;
    const phone_theme_t *th = phone_theme();
    ui_rect_t clip = ui_view_clip(v);
    ui_rect_t strip = form_strip(f);

    if (ui_intersects(clip, strip)) {
        /* Lifted fields must not spill into the header or action row */
        twin_rect_t saved = ui_clip_push(px, strip);
        ui_fill(px, strip, th->bg);
        for (int i = 0; i < FIELDS; i++) {
            ui_rect_t s = field_slot(f, i);
            if (!ui_intersects(s, clip) || !ui_intersects(s, strip))
                continue;
            ui_text(px, FONT_SM, FIELD_X + 2, s.top + 11, UI_LEFT,
                    f->focus == i ? th->primary : th->text_sec,
                    fields[i].label);
            ui_rect_t box = field_box(f, i);
            ui_field(px, box, &f->field[i], fields[i].placeholder);
            if (i == 0 && f->name_error && f->focus != 0)
                ui_rrect_stroke(px, box, 6, 1, th->missed_red);
            /* Length counter once the limit is near */
            if (f->focus == i && f->field[i].len > fields[i].max - 8) {
                char n[8] = {(char) ('0' + f->field[i].len / 10),
                             (char) ('0' + f->field[i].len % 10),
                             '/',
                             (char) ('0' + fields[i].max / 10),
                             (char) ('0' + fields[i].max % 10),
                             0};
                ui_text(px, FONT_SM, s.right - FIELD_X - 2, s.top + 11,
                        UI_RIGHT, th->text_sec, n);
            }
        }
        if (f->error && !f->kbd.visible)
            ui_text_in(px, FONT_SM, error_rect(), UI_CENTER, th->missed_red,
                       f->error);
        twin_pixmap_restore_clip(px, saved);
    }

    ui_rect_t actions = ui_rect(0, actions_top(f), PHONE_W, ACTIONS_H);
    if (!f->kbd.visible)
        actions.bottom = ui_view_height(v);
    if (ui_intersects(clip, actions)) {
        ui_fill(px, actions, th->bg);
        ui_fill(px, ui_rect(0, actions.top, PHONE_W, 1), th->separator);
        ui_button(px, action_rect(f, FORM_CANCEL), 10, UI_BTN_CARD,
                  f->pressed == FORM_CANCEL, FONT_NORMAL, "Cancel");
        ui_button(px, action_rect(f, FORM_SAVE), 10, UI_BTN_PRIMARY,
                  f->pressed == FORM_SAVE, FONT_NORMAL, "Save");
    }

    if (f->kbd.visible && ui_intersects(clip, kbd_rect(f)))
        ui_kbd_draw(px, &f->kbd, kbd_rect(f));

    ui_rect_t hdr = ui_rect(0, 0, PHONE_W, PHONE_HEADER_H);
    if (ui_intersects(clip, hdr))
        ui_header(px, hdr, "New Contact", NULL);
}

/* Keyboard mode suited to the field: digits for the phone number, and a
 * capital letter at the start of each word of a name. */
static void kbd_adapt(contact_form_t *f, bool focus_changed)
{
    const ui_field_t *fl = &f->field[f->focus];
    if (focus_changed)
        f->kbd.symbols = f->focus == 1;
    if (f->focus == 0 && !f->kbd.symbols)
        f->kbd.shift = !fl->len || fl->text[fl->len - 1] == ' ';
    else if (focus_changed)
        f->kbd.shift = false;
}

static void form_focus(contact_form_t *f, int i)
{
    if (i == f->focus)
        return;
    bool was_visible = f->kbd.visible;
    if (f->focus >= 0)
        f->field[f->focus].focused = false;
    f->focus = i;
    if (i >= 0) {
        f->field[i].focused = true;
        f->kbd.visible = true;
        kbd_adapt(f, true);
    } else {
        f->kbd.visible = false;
        f->kbd.pressed = -1;
    }
    if (was_visible != f->kbd.visible) {
        /* Keyboard shown or hidden: the whole layout reflows */
        ui_view_invalidate_all(&f->view);
    } else {
        /* Focus moved: the lifted strip and the keyboard mode changed */
        ui_view_invalidate(&f->view, form_strip(f));
        ui_view_invalidate(&f->view, kbd_rect(f));
    }
}

static void form_clear_error(contact_form_t *f)
{
    if (!f->error && !f->name_error)
        return;
    f->error = NULL;
    f->name_error = false;
    ui_view_invalidate(&f->view, error_rect());
    ui_view_invalidate(&f->view, field_box(f, 0));
}

static void form_type(contact_form_t *f, int key)
{
    if (f->focus < 0)
        return;
    ui_field_t *fl = &f->field[f->focus];
    if (key == UI_KEY_ENTER) {
        form_focus(f, f->focus + 1 < FIELDS ? f->focus + 1 : -1);
        return;
    }
    if (key != UI_KEY_BACKSPACE && fl->len >= fields[f->focus].max)
        return;
    if (!ui_field_key(fl, key))
        return;
    form_clear_error(f);
    /* The label line carries the length counter */
    ui_view_invalidate(&f->view, field_slot(f, f->focus));
    bool shift = f->kbd.shift;
    kbd_adapt(f, false);
    if (shift != f->kbd.shift)
        ui_view_invalidate(&f->view, kbd_rect(f));
}

static contacts_list_t *find_list(void);

/* Copy @s without leading/trailing spaces */
static void trimmed(char *dst, size_t len, const char *s)
{
    while (*s == ' ')
        s++;
    size_t n = strlen(s);
    while (n && s[n - 1] == ' ')
        n--;
    if (n >= len)
        n = len - 1;
    memcpy(dst, s, n);
    dst[n] = 0;
}

static void form_save(contact_form_t *f)
{
    char name[sizeof(f->field[0].text)], phone[sizeof(name)],
        email[sizeof(name)];
    trimmed(name, sizeof(name), f->field[0].text);
    trimmed(phone, sizeof(phone), f->field[1].text);
    trimmed(email, sizeof(email), f->field[2].text);

    int32_t idx = name[0] ? phone_contact_add(name, phone, email) : -1;
    if (idx < 0) {
        /* Hide the keyboard so the message below the fields is visible */
        form_focus(f, -1);
        f->name_error = !name[0];
        f->error = name[0] ? "Contact list full" : "Name required";
        ui_view_invalidate(&f->view, error_rect());
        ui_view_invalidate(&f->view, field_box(f, 0));
        return;
    }
    contacts_list_t *l = find_list();
    if (l)
        list_added(l, idx);
    ui_view_close(&f->view);
}

static int form_action_at(contact_form_t *f, twin_coord_t x, twin_coord_t y)
{
    if (ui_hit(action_rect(f, FORM_CANCEL), x, y))
        return FORM_CANCEL;
    if (ui_hit(action_rect(f, FORM_SAVE), x, y))
        return FORM_SAVE;
    return -1;
}

static int form_field_at(contact_form_t *f, twin_coord_t x, twin_coord_t y)
{
    if (!ui_hit(form_strip(f), x, y))
        return -1;
    for (int i = 0; i < FIELDS; i++)
        if (ui_hit(field_slot(f, i), x, y))
            return i;
    return -1;
}

static ui_rect_t form_action(ui_view_t *v, int which)
{
    return action_rect((contact_form_t *) v, which);
}

static void form_set_pressed(contact_form_t *f, int which)
{
    ui_view_set_pressed(&f->view, &f->pressed, which, form_action);
}

static bool form_event(ui_view_t *v, const ui_event_t *e)
{
    contact_form_t *f = (contact_form_t *) v;
    ui_rect_t kr = kbd_rect(f);

    if (e->kind == UI_KEY) {
        if (f->focus < 0)
            return false;
        form_type(f, (int) e->ucs4);
        return true;
    }

    /* Gestures that start on the keyboard belong to it */
    if (f->kbd.visible && ui_hit(kr, e->x0, e->y0)) {
        int key = ui_kbd_event(&f->kbd, kr, e);
        if (!ui_empty(f->kbd.dirty))
            ui_view_invalidate(v, f->kbd.dirty);
        if (key != UI_KEY_NONE && key != UI_KEY_SHIFT && key != UI_KEY_MODE)
            form_type(f, key);
        return true;
    }

    switch (e->kind) {
    case UI_PRESS:
    case UI_MOVE:
        form_set_pressed(f, form_action_at(f, e->x, e->y));
        return true;
    case UI_TAP: {
        int action = form_action_at(f, e->x, e->y);
        int field = form_field_at(f, e->x, e->y);
        form_set_pressed(f, -1);
        if (action == FORM_CANCEL)
            ui_view_close(v);
        else if (action == FORM_SAVE)
            form_save(f);
        else
            form_focus(f, field); /* outside every field: hide keyboard */
        return true;
    }
    default:
        form_set_pressed(f, -1);
        return true;
    }
}

static void form_open(void)
{
    contact_form_t *f =
        ui_view_new(sizeof(*f), "New Contact", form_paint, form_event, NULL);
    if (!f)
        return;
    ui_kbd_init(&f->kbd);
    f->focus = f->pressed = -1;
    ui_view_show(&f->view);
}

/* The list view under the form, looked up afresh (never cached) */
static contacts_list_t *find_list(void)
{
    twin_screen_t *s = phone_screen();
    contacts_list_t *found = NULL;
    for (twin_window_t *w = twin_screen_first_window(s); w;
         w = twin_window_next(w)) {
        ui_view_t *v = ui_view_of(w);
        if (v && v->paint == list_paint)
            found = (contacts_list_t *) v;
    }
    return found;
}

/*
 * Test hooks: inspect the contacts screen on top of the stack
 */
int app_contacts_test_offset(void)
{
    ui_view_t *v = phone_top();
    if (!v || v->paint != list_paint)
        return -1;
    return ((contacts_list_t *) v)->scroll.offset;
}

const char *app_contacts_test_detail(void)
{
    ui_view_t *v = phone_top();
    if (!v || v->paint != detail_paint)
        return NULL;
    const phone_contact_t *c =
        phone_contact_at((uint32_t) ((contact_detail_t *) v)->idx);
    return c ? c->name : NULL;
}

const char *app_contacts_test_form_error(void)
{
    ui_view_t *v = phone_top();
    if (!v || v->paint != form_paint)
        return NULL;
    contact_form_t *f = (contact_form_t *) v;
    return f->error ? f->error : "";
}

bool app_contacts_test_keyboard(void)
{
    ui_view_t *v = phone_top();
    return v && v->paint == form_paint && ((contact_form_t *) v)->kbd.visible;
}
