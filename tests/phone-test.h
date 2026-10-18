/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Shared helpers for the cellphone scenario suite.  Each app contributes
 * tests/phone-<app>.c defining test_<app>(); test-cellphone.c runs them in
 * a fixed order on one booted, unlocked phone sitting on the home screen.
 */

#ifndef _TWIN_PHONE_TEST_H_
#define _TWIN_PHONE_TEST_H_

#include "harness.h"
#include "phone.h"

extern th_t *phone_th;

/* Screen y of content y (content starts below the status bar) */
#define CY(y) ((twin_coord_t) ((y) + PHONE_STATUS_H))

/* Settle, check damage consistency, compare the golden CRC */
void ph_shot(const char *name);

/* Like ph_shot() for screens showing live counters: no golden CRC */
void ph_check(const char *name);

/* Open an app by registry name and settle; checks it is on top */
ui_view_t *ph_open(const char *name);

/* Return to the home screen and settle; checks the stack is empty */
void ph_home(void);

/* Tap at content coordinates and settle */
void ph_tap(twin_coord_t x, twin_coord_t y);

/* Drag at content coordinates and settle */
void ph_drag(twin_coord_t x0,
             twin_coord_t y0,
             twin_coord_t x1,
             twin_coord_t y1);

/* Tap the soft-keyboard key at its position in the letters layout, else in
 * the digits/symbols one (the keyboard must be showing that layout) */
void ph_key(int code);

/* Settle animations and timers for @ms of virtual time */
void ph_wait(int ms);

/* App suites */
void test_dialer(void);
void test_calllog(void);
void test_contacts(void);
void test_sms(void);
void test_music(void);
void test_photos(void);
void test_camera(void);
void test_settings(void);
void test_games(void);
void test_scenes(void);
void test_demos(void);
void test_regress(void);

#endif /* _TWIN_PHONE_TEST_H_ */
