/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

#define _twin_button_bw(button) ((button)->label.font_size / 5)

static void _twin_button_paint(twin_button_t *button)
{
    _twin_widget_bevel(&button->label.widget, _twin_button_bw(button),
                       button->active);
}

static void _twin_button_set_label_offset(twin_button_t *button)
{
    twin_fixed_t bf = _twin_button_bw(button);
    twin_fixed_t bh = bf / 2;

    if (button->active)
        button->label.offset.y = button->label.offset.x = 0;
    else
        button->label.offset.y = button->label.offset.x = -bh;
    _twin_widget_queue_paint(&button->label.widget);
}

twin_dispatch_result_t _twin_button_dispatch(twin_widget_t *widget,
                                             twin_event_t *event,
                                             void *closure)
{
    twin_button_t *button = (twin_button_t *) widget;

    if (_twin_label_dispatch(widget, event, closure) == TwinDispatchDone)
        return TwinDispatchDone;
    switch (event->kind) {
    case TwinEventPaint:
        _twin_button_paint(button);
        break;
    case TwinEventButtonDown:
        button->pressed = true;
        button->active = true;
        _twin_button_set_label_offset(button);

        /* Invoke widget callback for button press */
        if (widget->callback) {
            twin_event_t press_event = *event;
            press_event.kind = TwinEventButtonSignalDown;
            press_event.u.button_signal.signal = TwinButtonSignalDown;
            (*widget->callback)(widget, &press_event, widget->callback_data);
        }
        return TwinDispatchDone;
        break;
    case TwinEventMotion:
        if (button->pressed) {
            bool active = _twin_widget_contains(
                &button->label.widget, event->u.pointer.x, event->u.pointer.y);
            if (active != button->active) {
                button->active = active;
                _twin_button_set_label_offset(button);
            }
        }
        return TwinDispatchDone;
        break;
    case TwinEventButtonUp:
        button->pressed = false;
        if (button->active) {
            button->active = false;
            _twin_button_set_label_offset(button);

            /* Invoke widget callback for button release (click) */
            if (widget->callback) {
                twin_event_t release_event = *event;
                release_event.kind = TwinEventButtonSignalUp;
                release_event.u.button_signal.signal = TwinButtonSignalUp;
                (*widget->callback)(widget, &release_event,
                                    widget->callback_data);
            }
        }
        return TwinDispatchDone;
        break;
    default:
        break;
    }
    return TwinDispatchContinue;
}

void _twin_button_init(twin_button_t *button,
                       twin_box_t *parent,
                       const char *value,
                       twin_argb32_t foreground,
                       twin_fixed_t font_size,
                       twin_style_t font_style,
                       twin_widget_proc_t handler)
{
    _twin_label_init(&button->label, parent, value, foreground, font_size,
                     font_style, handler);
    button->pressed = false;
    button->active = false;
    _twin_button_set_label_offset(button);
}


twin_button_t *twin_button_create(twin_box_t *parent,
                                  const char *value,
                                  twin_argb32_t foreground,
                                  twin_fixed_t font_size,
                                  twin_style_t font_style)
{
    twin_button_t *button = malloc(sizeof(twin_button_t));
    if (!button)
        return NULL;

    _twin_button_init(button, parent, value, foreground, font_size, font_style,
                      _twin_button_dispatch);
    return button;
}
