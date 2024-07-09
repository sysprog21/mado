/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#ifndef _APPS_CALC_H_
#define _APPS_CALC_H_

#include <twin.h>

void apps_calc_start(twin_screen_t *screen,
                     const char *name,
                     int x,
                     int y,
                     int w,
                     int h);

#endif /* _APPS_CALC_H_ */
