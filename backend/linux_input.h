/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#ifndef _LINUX_INPUT_H__
#define _LINUX_INPUT_H__

#include <twin.h>

/* Mouse and touch input from evdev, with udev hotplug.  Everything runs on
 * the thread that calls twin_linux_input_poll(), the one that runs the
 * screen: no locking, no extra thread. */
typedef struct twin_linux_input twin_linux_input_t;

twin_linux_input_t *twin_linux_input_create(twin_screen_t *screen);

void twin_linux_input_destroy(twin_linux_input_t *tm);

/* Wait up to @timeout_ms (-1: until input arrives) for input, then
 * dispatch whatever arrived to the screen */
void twin_linux_input_poll(twin_linux_input_t *tm, int timeout_ms);

#endif
