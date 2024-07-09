/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#ifndef _TWIN_CLOCK_H_
#define _TWIN_CLOCK_H_

#include <twin.h>

typedef struct _twin_clock {
    twin_widget_t   widget;
    twin_timeout_t  *timeout;
} twin_clock_t;

void
_twin_clock_paint (twin_clock_t *clock);

twin_dispatch_result_t
_twin_clock_dispatch (twin_widget_t *widget, twin_event_t *event);
    
void
_twin_clock_init (twin_clock_t		*clock, 
		  twin_box_t		*parent,
		  twin_dispatch_proc_t	dispatch);

twin_clock_t *
twin_clock_create (twin_box_t *parent);

void
twin_clock_start (twin_screen_t *screen, const char *name, int x, int y, int w, int h);

#endif /* _TWIN_CLOCK_H_ */
