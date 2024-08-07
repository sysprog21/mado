/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#ifndef _LINUX_INPUT_H__
#define _LINUX_INPUT_H__

void *twin_linux_input_create(twin_screen_t *screen);

void twin_linux_input_destroy(void *tm);

#endif
