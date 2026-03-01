/*
 * Twin - A Tiny Window System
 * Copyright (c) 2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#ifndef _APPS_LOTTIE_H_
#define _APPS_LOTTIE_H_

#include <twin.h>

/**
 * Start Lottie animation player
 *
 * @param screen Screen to create window on
 * @param name   Window title
 * @param path   Path to Lottie file (.json or .lottie)
 * @param x      Window X position
 * @param y      Window Y position
 */
void apps_lottie_start(twin_screen_t *screen,
                       const char *name,
                       const char *path,
                       int x,
                       int y);

#endif /* _APPS_LOTTIE_H_ */