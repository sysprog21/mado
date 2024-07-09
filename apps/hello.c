/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <string.h>
#include <time.h>

#include "apps_hello.h"

#define maybe_unused __attribute__((unused))

static twin_time_t _apps_hello_timeout(twin_time_t maybe_unused now,
                                       void *closure)
{
    twin_label_t *labelb = closure;
    time_t secs = time(0);
    char *t = ctime(&secs);

    *strchr(t, '\n') = '\0';
    twin_label_set(labelb, t, 0xff008000, twin_int_to_fixed(12),
                   TWIN_TEXT_OBLIQUE);
    return 1000;
}

void apps_hello_start(twin_screen_t *screen,
                      const char *name,
                      int x,
                      int y,
                      int w,
                      int h)
{
    twin_toplevel_t *top = twin_toplevel_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, name);
    twin_label_t *labela = twin_label_create(
        &top->box, name, 0xff000080, twin_int_to_fixed(12), TWIN_TEXT_ROMAN);
    twin_widget_t *widget =
        twin_widget_create(&top->box, 0xff800000, 1, 2, 0, 0);
    twin_label_t *labelb = twin_label_create(
        &top->box, name, 0xff008000, twin_int_to_fixed(12), TWIN_TEXT_OBLIQUE);
    twin_button_t *button = twin_button_create(
        &top->box, "Button", 0xff800000, twin_int_to_fixed(18),
        TWIN_TEXT_BOLD | TWIN_TEXT_OBLIQUE);
    twin_widget_set(&labela->widget, 0xc0c0c0c0);
    (void) widget;
    twin_widget_set(&labelb->widget, 0xc0c0c0c0);
    twin_widget_set(&button->label.widget, 0xc0808080);
    twin_toplevel_show(top);
    twin_set_timeout(_apps_hello_timeout, 1000, labelb);
}
