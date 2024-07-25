/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#ifndef _APPS_MULTI_H_
#define _APPS_MULTI_H_

#include <twin.h>

void apps_multi_start(twin_screen_t *screen,
                      const char *name,
                      int x,
                      int y,
                      int w,
                      int h);

#endif /* _APPS_MULTI_H_ */
