/*
 * $Id$
 *
 * Copyright © 2004 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "twin_calc.h"
#include <stdio.h>

#define TWIN_CALC_STACK	5
#define TWIN_CALC_ZERO	    0
#define TWIN_CALC_ONE	    1
#define TWIN_CALC_TWO	    2
#define TWIN_CALC_THREE	    3
#define TWIN_CALC_FOUR	    4
#define TWIN_CALC_FIVE	    5
#define TWIN_CALC_SIX	    6
#define TWIN_CALC_SEVEN	    7
#define TWIN_CALC_EIGHT	    8
#define TWIN_CALC_NINE	    9
#define TWIN_CALC_PLUS	    10
#define TWIN_CALC_MINUS	    11
#define TWIN_CALC_TIMES	    12
#define TWIN_CALC_DIVIDE    13
#define TWIN_CALC_EQUAL	    14
#define TWIN_CALC_CLEAR	    15
#define TWIN_CALC_NBUTTON   16

/*
 * Layout:
 *
 *	    display
 *
 *     7  8  9 +
 *     4  5  6 -
 *     1  2  3 *
 *     0 clr = ÷
 */
#define TWIN_CALC_COLS	4
#define TWIN_CALC_ROWS	4

const static int    calc_layout[TWIN_CALC_ROWS][TWIN_CALC_COLS] = {
    { TWIN_CALC_SEVEN, TWIN_CALC_EIGHT, TWIN_CALC_NINE,  TWIN_CALC_PLUS },
    { TWIN_CALC_FOUR,  TWIN_CALC_FIVE,  TWIN_CALC_SIX,   TWIN_CALC_MINUS },
    { TWIN_CALC_ONE,   TWIN_CALC_TWO,   TWIN_CALC_THREE, TWIN_CALC_TIMES },
    { TWIN_CALC_ZERO,  TWIN_CALC_CLEAR, TWIN_CALC_EQUAL, TWIN_CALC_DIVIDE },
};

typedef struct _twin_calc {
    int    	    stack[TWIN_CALC_STACK];
    int		    pending_op;
    twin_bool_t	    pending_delete;
    twin_window_t   *window;
    twin_toplevel_t *toplevel;
    twin_box_t	    *keys;
    twin_box_t	    *cols[4];
    twin_label_t    *display;
    twin_button_t   *buttons[TWIN_CALC_NBUTTON];
} twin_calc_t;

static const char *twin_calc_labels[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "+", "-", "*", "/", "=", "CLR"
};

#define TWIN_CALC_VALUE_SIZE	twin_int_to_fixed(24)
#define TWIN_CALC_VALUE_STYLE	TWIN_TEXT_BOLD
#define TWIN_CALC_VALUE_FG	0xff000000
#define TWIN_CALC_VALUE_BG	0xc0c0c0c0
#define TWIN_CALC_BUTTON_SIZE	twin_int_to_fixed(15)
#define TWIN_CALC_BUTTON_STYLE	TWIN_TEXT_BOLD
#define TWIN_CALC_BUTTON_FG	0xff000000
#define TWIN_CALC_BUTTON_BG	0xc0808080

static int
_twin_calc_button_to_id (twin_calc_t *calc, twin_button_t *button)
{
    int	i;

    for (i = 0; i < TWIN_CALC_NBUTTON; i++)
	if (calc->buttons[i] == button)
	    return i;
    return -1;
}

static void
_twin_calc_update_value (twin_calc_t *calc)
{
    char    v[20];

    sprintf (v, "%d", calc->stack[0]);
    twin_label_set (calc->display, v, TWIN_CALC_VALUE_FG,
		    TWIN_CALC_VALUE_SIZE, TWIN_CALC_VALUE_STYLE);
}

static void
_twin_calc_push (twin_calc_t *calc)
{
    int	i;

    for (i = 0; i < TWIN_CALC_STACK - 1; i++)
	calc->stack[i+1] = calc->stack[i];
    calc->pending_delete = TWIN_TRUE;
}

static int
_twin_calc_pop (twin_calc_t *calc)
{
    int i;
    int	v = calc->stack[0];
    for (i = 0; i < TWIN_CALC_STACK - 1; i++)
	calc->stack[i] = calc->stack[i+1];
    return v;
}
    
static void
_twin_calc_digit (twin_calc_t *calc, int digit)
{
    if (calc->pending_delete)
    {
	calc->stack[0] = 0;
	calc->pending_delete = TWIN_FALSE;
    }
    calc->stack[0] = calc->stack[0] * 10 + digit;
    _twin_calc_update_value (calc);
}

static void
_twin_calc_button_signal (twin_button_t		*button,
			  twin_button_signal_t	signal,
			  void			*closure)
{
    twin_calc_t	*calc = closure;
    int		i;
    int		a, b;

    if (signal != TwinButtonSignalDown) return;
    i = _twin_calc_button_to_id (calc, button);
    if (i < 0) return;
    switch (i) {
    case TWIN_CALC_PLUS:
    case TWIN_CALC_MINUS:
    case TWIN_CALC_TIMES:
    case TWIN_CALC_DIVIDE:
	calc->pending_op = i;
	_twin_calc_push (calc);
	break;
    case TWIN_CALC_EQUAL:
	if (calc->pending_op != 0)
	{
	    b = _twin_calc_pop (calc);
	    a = _twin_calc_pop (calc);
	    switch (calc->pending_op) {
	    case TWIN_CALC_PLUS:    	a = a + b;  break;
	    case TWIN_CALC_MINUS:    	a = a - b;  break;
	    case TWIN_CALC_TIMES:    	a = a * b;  break;
	    case TWIN_CALC_DIVIDE:    	if (!b) a = 0; else a = a / b; break;
	    }
	    _twin_calc_push (calc);
	    calc->stack[0] = a;
	    _twin_calc_update_value (calc);
	    calc->pending_op = 0;
	}
        calc->pending_delete = TWIN_TRUE;
	break;
    case TWIN_CALC_CLEAR:
	for (i = 0; i < TWIN_CALC_STACK; i++)
	    calc->stack[i] = 0;
        calc->pending_op = 0;
        calc->pending_delete = TWIN_TRUE;
	_twin_calc_update_value (calc);
	break;
    default:
	_twin_calc_digit (calc, i);
	break;
    }
}

void
twin_calc_start (twin_screen_t *screen, const char *name, int x, int y, int w, int h)
{
    twin_calc_t	*calc = malloc (sizeof (twin_calc_t));
    int		i, j;

    calc->toplevel = twin_toplevel_create (screen, 
					   TWIN_ARGB32,
					   TwinWindowApplication,
					   x, y, w, h, name);
    calc->display = twin_label_create (&calc->toplevel->box,
				       "0",
				       TWIN_CALC_VALUE_FG,
				       TWIN_CALC_VALUE_SIZE,
				       TWIN_CALC_VALUE_STYLE);
    twin_widget_set (&calc->display->widget, TWIN_CALC_VALUE_BG);
    calc->keys = twin_box_create (&calc->toplevel->box, TwinBoxHorz);
    for (i = 0; i < TWIN_CALC_COLS; i++)
    {
	calc->cols[i] = twin_box_create (calc->keys, TwinBoxVert);
	for (j = 0; j < TWIN_CALC_ROWS; j++)
	{
	    int	b = calc_layout[j][i];
	    calc->buttons[b] = twin_button_create (calc->cols[i],
						   twin_calc_labels[b],
						   TWIN_CALC_BUTTON_FG,
						   TWIN_CALC_BUTTON_SIZE,
						   TWIN_CALC_BUTTON_STYLE);
	    twin_widget_set (&calc->buttons[b]->label.widget,
			     TWIN_CALC_BUTTON_BG);
	    calc->buttons[b]->signal = _twin_calc_button_signal;
	    calc->buttons[b]->closure = calc;
	    if (i || j)
		calc->buttons[b]->label.widget.copy_geom = &calc->buttons[calc_layout[0][0]]->label.widget;
	}
    }
	    
    for (i = 0; i < TWIN_CALC_STACK; i++)
	calc->stack[i] = 0;
    calc->pending_delete = TWIN_TRUE;
    calc->pending_op = 0;
    twin_toplevel_show (calc->toplevel);
}
