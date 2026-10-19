/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Messages: thread list, chat scrolling, composer and soft keyboard.
 */

#include <string.h>

#include "data.h"
#include "phone-test.h"

int app_sms_test_offset(void);
int app_sms_test_max_offset(void);
int app_sms_test_messages(void);
int app_sms_test_keyboard(void);
const char *app_sms_test_field(void);
const char *app_sms_test_message(int i);

/* Layout mirrored from sms.c (content coordinates) */
#define ROW_H 52
#define COMPOSER_H 36

static twin_coord_t composer_cy(bool kbd)
{
    return PHONE_CONTENT_H - COMPOSER_H / 2 - (kbd ? UI_KBD_H : 0);
}

#define SEND_X (PHONE_W - 6 - 20)
#define FIELD_X 90

void test_sms(void)
{
    const phone_sms_thread_t *threads = phone_sms_threads();

    ph_open("Messages");
    CHECK_EQ(phone_depth(), 1, "thread list open");
    CHECK_EQ(app_sms_test_offset(), -1, "list is not a chat");
    ph_shot("sms-list");

    /* Fourth thread: the longest conversation, so it scrolls */
    ph_tap(120, PHONE_HEADER_H + 3 * ROW_H + ROW_H / 2);
    CHECK_EQ(phone_depth(), 2, "chat pushed over the list");
    ui_view_t *top = phone_top();
    CHECK_STR(top ? top->title : NULL, threads[3].contact_name, "chat title");
    int stock = app_sms_test_messages();
    CHECK_EQ(stock, (int) threads[3].msg_count, "stock messages shown");
    int max = app_sms_test_max_offset();
    CHECK(max > 0, "conversation overflows (max %d)", max);
    CHECK_EQ(app_sms_test_offset(), max, "opens at the newest message");
    CHECK_EQ(app_sms_test_keyboard(), 0, "keyboard hidden on open");
    ph_shot("sms-chat");

    /* Dragging down reveals older messages, and is not a tap */
    ph_drag(120, 60, 120, 160);
    CHECK(app_sms_test_offset() < max, "dragged to %d", app_sms_test_offset());
    CHECK_EQ(app_sms_test_keyboard(), 0, "drag does not open the keyboard");

    /* Send is disabled while the field is empty */
    ph_tap(SEND_X, composer_cy(false));
    CHECK_EQ(app_sms_test_messages(), stock, "empty send ignored");

    /* Field tap raises the keyboard and re-anchors at the newest message */
    ph_tap(FIELD_X, composer_cy(false));
    CHECK_EQ(app_sms_test_keyboard(), 1, "field tap shows the keyboard");
    CHECK_EQ(app_sms_test_offset(), app_sms_test_max_offset(),
             "latest message visible above the composer");
    ph_tap(SEND_X, composer_cy(true));
    CHECK_EQ(app_sms_test_messages(), stock,
             "empty send ignored with keyboard");

    ph_key('h');
    ph_key('i');
    const char *f = app_sms_test_field();
    CHECK_STR(f, "hi", "typed on the soft keyboard");
    ph_shot("sms-typing");

    ph_tap(SEND_X, composer_cy(true));
    CHECK_EQ(app_sms_test_messages(), stock + 1, "send appends a message");
    const char *m = app_sms_test_message(stock);
    CHECK_STR(m, "hi", "message sent");
    f = app_sms_test_field();
    CHECK(f && !f[0], "field cleared after send");
    CHECK_EQ(app_sms_test_keyboard(), 1, "keyboard stays open after send");
    CHECK_EQ(app_sms_test_offset(), app_sms_test_max_offset(),
             "scrolled to the new message");
    ph_shot("sms-sent");

    /* Hardware keys type into the field; Enter sends */
    th_key(phone_th, 'o');
    th_key(phone_th, 'k');
    ph_wait(100);
    f = app_sms_test_field();
    CHECK_STR(f, "ok", "typed on the hardware keyboard");
    th_key(phone_th, '\r');
    ph_wait(500);
    CHECK_EQ(app_sms_test_messages(), stock + 2, "Enter sends");

    /* A drag in the shrunken chat area scrolls without dismissing */
    ph_drag(120, PHONE_HEADER_H + 5, 120, PHONE_HEADER_H + 55);
    CHECK_EQ(app_sms_test_keyboard(), 1, "scroll keeps the keyboard");

    /* Tapping the conversation outside the composer dismisses it */
    ph_tap(120, PHONE_HEADER_H + 20);
    CHECK_EQ(app_sms_test_keyboard(), 0, "outside tap hides the keyboard");
    CHECK_EQ(app_sms_test_offset(), app_sms_test_max_offset(),
             "newest message stays in view after the reflow");

    /* Theme switch repaints from state */
    phone_theme_set(1);
    ph_shot("sms-chat-dark");
    phone_theme_set(0);
    ph_wait(100);

    /* Nav bar Back pops to the list */
    th_tap(phone_th, PHONE_W / 2, PHONE_H - PHONE_NAV_H / 2);
    ph_wait(2000);
    CHECK_EQ(phone_depth(), 1, "Back returns to the list");
    top = phone_top();
    CHECK_STR(top ? top->title : NULL, "Messages", "list on top");

    ph_home();
}
