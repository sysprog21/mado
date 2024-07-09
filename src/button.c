/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twinint.h"

#define _twin_button_bw(button)	((button)->label.font_size / 5)

static void
_twin_button_paint (twin_button_t *button)
{
    _twin_widget_bevel (&button->label.widget, 
			_twin_button_bw(button),
			button->active);
}

static void
_twin_button_set_label_offset (twin_button_t *button)
{
    twin_fixed_t    bf = _twin_button_bw (button);
    twin_fixed_t    bh = bf / 2;

    if (button->active)
	button->label.offset.y = button->label.offset.x = 0;
    else
	button->label.offset.y = button->label.offset.x = -bh;
    _twin_widget_queue_paint (&button->label.widget);
}

twin_dispatch_result_t
_twin_button_dispatch (twin_widget_t *widget, twin_event_t *event)
{
    twin_button_t    *button = (twin_button_t *) widget;

    if (_twin_label_dispatch (widget, event) == TwinDispatchDone)
	return TwinDispatchDone;
    switch (event->kind) {
    case TwinEventPaint:
	_twin_button_paint (button);
	break;
    case TwinEventButtonDown:
	button->pressed = TWIN_TRUE;
	button->active = TWIN_TRUE;
	_twin_button_set_label_offset (button);
	if (button->signal)
	    (*button->signal) (button, TwinButtonSignalDown, button->closure);
	return TwinDispatchDone;
	break;
    case TwinEventMotion:
	if (button->pressed)
	{
	    twin_bool_t	active = _twin_widget_contains (&button->label.widget,
							event->u.pointer.x,
							event->u.pointer.y);
	    if (active != button->active)
	    {
		button->active = active;
		_twin_button_set_label_offset (button);
	    }
	}
	return TwinDispatchDone;
	break;
    case TwinEventButtonUp:
	button->pressed = TWIN_FALSE;
	if (button->active)
	{
	    button->active = TWIN_FALSE;
	    _twin_button_set_label_offset (button);
	    if (button->signal)
		(*button->signal) (button, TwinButtonSignalUp, button->closure);
	}
	return TwinDispatchDone;
	break;
    default:
	break;
    }
    return TwinDispatchContinue;
}

void
_twin_button_init (twin_button_t	*button,
		   twin_box_t		*parent,
		   const char		*value,
		   twin_argb32_t	foreground,
		   twin_fixed_t		font_size,
		   twin_style_t		font_style,
		   twin_dispatch_proc_t	dispatch)
{
    _twin_label_init (&button->label, parent, value,
		      foreground, font_size, font_style, dispatch);
    button->pressed = TWIN_FALSE;
    button->active = TWIN_FALSE;
    button->signal = NULL;
    button->closure = NULL;
    _twin_button_set_label_offset (button);
}


twin_button_t *
twin_button_create (twin_box_t	    *parent,
		   const char	    *value,
		   twin_argb32_t    foreground,
		   twin_fixed_t	    font_size,
		   twin_style_t	    font_style)
{
    twin_button_t    *button = malloc (sizeof (twin_button_t));

    if (!button)
	return 0;
    _twin_button_init (button, parent, value, foreground, 
		       font_size, font_style, _twin_button_dispatch);
    return button;
}

