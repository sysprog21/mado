/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University
 * All rights reserved.
 */

#ifndef _APPS_ANIMATION_H_
#define _APPS_ANIMATION_H_

#include <stdbool.h>
#include <twin.h>

bool apps_animation_start(twin_screen_t *screen,
                          const char *name,
                          const char *path,
                          int x,
                          int y);

#endif /* _APPS_ANIMATION_H_ */
