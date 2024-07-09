/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#ifndef _TWIN_DEMOSPLINE_H_
#define _TWIN_DEMOSPLINE_H_

#include <twin.h>

#define NPT 4

typedef struct _twin_demospline {
    twin_widget_t widget;
    twin_point_t points[NPT];
    int which;
    twin_fixed_t line_width;
    twin_cap_t cap_style;
} twin_demospline_t;

void _twin_demospline_paint(twin_demospline_t *demospline);

twin_dispatch_result_t _twin_demospline_dispatch(twin_widget_t *widget,
                                                 twin_event_t *event);

void _twin_demospline_init(twin_demospline_t *demospline,
                           twin_box_t *parent,
                           twin_dispatch_proc_t dispatch);

twin_demospline_t *twin_demospline_create(twin_box_t *parent);

void twin_demospline_start(twin_screen_t *screen,
                           const char *name,
                           int x,
                           int y,
                           int w,
                           int h);

#endif /* _TWIN_DEMOLINE_H_ */
