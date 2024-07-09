/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#ifndef _TWIN_DEMOLINE_H_
#define _TWIN_DEMOLINE_H_

#include <twin.h>

typedef struct _twin_demoline {
    twin_widget_t   widget;
    twin_point_t    points[2];
    int		    which;
    twin_fixed_t    line_width;
    twin_cap_t	    cap_style;
} twin_demoline_t;

void
_twin_demoline_paint (twin_demoline_t *demoline);

twin_dispatch_result_t
_twin_demoline_dispatch (twin_widget_t *widget, twin_event_t *event);
    
void
_twin_demoline_init (twin_demoline_t		*demoline, 
		  twin_box_t		*parent,
		  twin_dispatch_proc_t	dispatch);

twin_demoline_t *
twin_demoline_create (twin_box_t *parent);

void
twin_demoline_start (twin_screen_t *screen, const char *name, int x, int y, int w, int h);

#endif /* _TWIN_DEMOLINE_H_ */
