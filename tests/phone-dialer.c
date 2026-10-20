/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/* Phone dialer, in-call screen and Call Log scenarios */

#include <string.h>

#include "phone-test.h"

#if defined(CONFIG_DEMO_PHONE_DIALER)

const char *app_dialer_test_number(void);
const char *app_dialer_test_name(void);
int app_dialer_test_seconds(void);
int app_dialer_test_toggles(void);
int app_calllog_test_offset(void);

/* Keypad geometry mirrors apps/cellphone/dialer.c (content coordinates) */
#define KEY_X(col) ((col) * 80 + 40)
#define KEY_Y(row) (50 + (row) * 40 + 20)
#define DEL_X 41
#define CALL_X 158
#define BOTTOM_Y 234
#define MUTE_X 48
#define SPEAKER_X 192
#define TOGGLE_Y 204
#define END_X 120
#define END_Y 204

static void tap_digit(char ch)
{
    static const char layout[] = "123456789*0#";
    const char *p = strchr(layout, ch);
    CHECK(p && *p, "digit %c on the keypad", ch);
    if (!p || !*p)
        return;
    int i = (int) (p - layout);
    ph_tap(KEY_X(i % 3), KEY_Y(i / 3));
}

static bool number_is(const char *want)
{
    const char *got = app_dialer_test_number();
    return got && !strcmp(got, want);
}

void test_dialer(void)
{
    ph_open("Phone");
    ph_shot("dialer-empty");

    /* Calling with nothing dialed does nothing */
    ph_tap(CALL_X, BOTTOM_Y);
    CHECK_EQ(phone_depth(), 1, "empty call stays on the keypad");
    ph_tap(DEL_X, BOTTOM_Y);
    CHECK(number_is(""), "backspace on empty is harmless");

    for (const char *s = "15550101"; *s; s++)
        tap_digit(*s);
    CHECK(number_is("15550101"), "typed digits: %s",
          app_dialer_test_number() ? app_dialer_test_number() : "(null)");
    ph_shot("dialer-number");

    ph_tap(DEL_X, BOTTOM_Y);
    CHECK(number_is("1555010"), "backspace drops the last digit");
    tap_digit('2');
    tap_digit('*');
    tap_digit('#');
    CHECK(number_is("15550102*#"), "star and hash typed");

    /* Hardware keys: backspace twice, then Enter places the call */
    th_key(phone_th, 8);
    th_key(phone_th, 8);
    ph_wait(100);
    CHECK(number_is("15550102"), "keyboard backspace");

    /* Non-ASCII code points must not alias onto keys: U+0131 narrows to
     * '1' and U+012B to '+' */
    th_key(phone_th, 0x131);
    th_key(phone_th, 0x12B);
    ph_wait(100);
    CHECK(number_is("15550102"), "non-ASCII keys ignored");

    /* The number fills up at 20 digits and the display truncates */
    for (int i = 0; i < 14; i++)
        th_key(phone_th, '9');
    ph_wait(100);
    CHECK(number_is("15550102999999999999"), "number capped at 20 digits");
    ph_shot("dialer-long");
    for (int i = 0; i < 12; i++)
        th_key(phone_th, 8);
    ph_wait(100);
    CHECK(number_is("15550102"), "trimmed back");

    /* Call: the in-call screen pushes on top of the keypad */
    ph_tap(CALL_X, BOTTOM_Y);
    CHECK_EQ(phone_depth(), 2, "call screen pushed");
    CHECK(number_is("15550102"), "call carries the dialed number");
    const char *name = app_dialer_test_name();
    CHECK_STR(name, "Brian Smith", "number resolved to a contact");
    int s0 = app_dialer_test_seconds();
    CHECK(s0 >= 0, "call timer running");
    ph_wait(3000);
    int s1 = app_dialer_test_seconds();
    CHECK(s1 - s0 >= 2 && s1 - s0 <= 4, "timer advanced %d -> %d", s0, s1);

    ph_tap(MUTE_X, TOGGLE_Y);
    CHECK_EQ(app_dialer_test_toggles(), 1, "mute on");
    ph_tap(SPEAKER_X, TOGGLE_Y);
    CHECK_EQ(app_dialer_test_toggles(), 3, "speaker on");
    ph_tap(MUTE_X, TOGGLE_Y);
    CHECK_EQ(app_dialer_test_toggles(), 2, "mute off");
    ph_shot("dialer-incall");

    ph_tap(END_X, END_Y);
    CHECK_EQ(phone_depth(), 1, "end call returns to the keypad");
    CHECK(number_is("15550102"), "keypad keeps the number after the call");

    /* An unknown number shows "Unknown" with a generic avatar */
    for (int i = 0; i < 8; i++)
        th_key(phone_th, 8);
    for (const char *s = "0800123"; *s; s++)
        th_key(phone_th, (twin_ucs4_t) *s);
    th_key(phone_th, '\r');
    ph_wait(500);
    CHECK_EQ(phone_depth(), 2, "Enter places the call");
    name = app_dialer_test_name();
    CHECK(name && !name[0], "unknown number has no contact name");
    CHECK(number_is("0800123"), "unknown number dialed");
    ph_shot("dialer-incall-unknown");

    /* Nav-bar Back hangs up too */
    phone_back();
    ph_wait(2000);
    CHECK_EQ(phone_depth(), 1, "back ends the call");
    ph_home();
}

#if defined(CONFIG_DEMO_PHONE_CALLLOG)
void test_calllog(void)
{
    ph_open("Call Log");
    ph_shot("calllog");
    CHECK_EQ(app_calllog_test_offset(), 0, "list starts at the top");

    /* Drag up: the list overflows (15 rows of 40 px in 228 px) */
    ph_drag(120, 220, 120, 80);
    int off = app_calllog_test_offset();
    CHECK(off > 100, "drag scrolled the list to %d", off);
    CHECK_EQ(phone_depth(), 1, "scrolling is not a tap");

    /* Fling to the bottom, then check the last rows render cleanly */
    ph_drag(120, 240, 120, 40);
    ph_drag(120, 240, 120, 40);
    off = app_calllog_test_offset();
    CHECK_EQ(off, 15 * 40 - (258 - 30), "scrolled to the end");
    ph_shot("calllog-end");

    /* Back to the top and tap the third row: the missed unknown caller */
    ph_drag(120, 40, 120, 250);
    ph_drag(120, 40, 120, 250);
    CHECK_EQ(app_calllog_test_offset(), 0, "back at the top");
    ph_tap(120, 30 + 2 * 40 + 20);
    CHECK_EQ(phone_depth(), 2, "row tap opens the call screen");
    const char *num = app_dialer_test_number();
    CHECK_STR(num, "+1-555-9999", "number called");
    ph_tap(END_X, END_Y);
    CHECK_EQ(phone_depth(), 1, "end call returns to the log");

    /* First row: a known contact */
    ph_tap(120, 30 + 20);
    const char *name = app_dialer_test_name();
    CHECK_STR(name, "Ashley Johnson", "contact called");
    CHECK(app_dialer_test_seconds() >= 0, "call timer running");
    ph_shot("calllog-call");
    ph_tap(END_X, END_Y);
    CHECK_EQ(phone_depth(), 1, "hung up");
    ph_home();
}
#else
void test_calllog(void) {}
#endif
#else
void test_dialer(void) {}
void test_calllog(void) {}
#endif
