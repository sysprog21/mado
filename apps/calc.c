/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>

#include "apps_calc.h"

#define APPS_CALC_STACK 5

enum {
#define _(x) APPS_CALC_##x
    _(ZERO),
    _(ONE),
    _(TWO),
    _(THREE),
    _(FOUR),
    _(FIVE),
    _(SIX),
    _(SEVEN),
    _(EIGHT),
    _(NINE),
    _(PLUS),
    _(MINUS),
    _(TIMES),
    _(DIVIDE),
    _(EQUAL),
    _(CLEAR),
    _(N_BUTTONS),
#undef _
};

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
#define APPS_CALC_COLS 4
#define APPS_CALC_ROWS 4

static const int calc_layout[APPS_CALC_ROWS][APPS_CALC_COLS] = {
#define _(x) APPS_CALC_##x
    {_(SEVEN), _(EIGHT), _(NINE), _(PLUS)},
    {_(FOUR), _(FIVE), _(SIX), _(MINUS)},
    {_(ONE), _(TWO), _(THREE), _(TIMES)},
    {_(ZERO), _(CLEAR), _(EQUAL), _(DIVIDE)},
#undef _
};

typedef struct _twin_calc {
    int stack[APPS_CALC_STACK];
    int pending_op;
    bool pending_delete;
    twin_window_t *window;
    twin_toplevel_t *toplevel;
    twin_box_t *keys;
    twin_box_t *cols[4];
    twin_label_t *display;
    twin_button_t *buttons[APPS_CALC_N_BUTTONS];
} apps_calc_t;

static const char *apps_calc_labels[] = {
    "0", "1", "2", "3", "4", "5", "6", "7",
    "8", "9", "+", "-", "*", "/", "=", "CLR",
};

#define APPS_CALC_VALUE_SIZE twin_int_to_fixed(29)
#define APPS_CALC_VALUE_STYLE TWIN_TEXT_ROMAN
#define APPS_CALC_VALUE_FG 0xff000000
#define APPS_CALC_VALUE_BG 0x80808080
#define APPS_CALC_BUTTON_SIZE twin_int_to_fixed(15)
#define APPS_CALC_BUTTON_STYLE TWIN_TEXT_BOLD
#define APPS_CALC_BUTTON_FG 0xff000000
#define APPS_CALC_BUTTON_BG 0xc0808080

static int _apps_calc_button_to_id(apps_calc_t *calc, twin_button_t *button)
{
    for (int i = 0; i < APPS_CALC_N_BUTTONS; i++)
        if (calc->buttons[i] == button)
            return i;
    return -1;
}

static void _apps_calc_update_value(apps_calc_t *calc)
{
    char v[20];

    sprintf(v, "%d", calc->stack[0]);
    twin_label_set(calc->display, v, APPS_CALC_VALUE_FG, APPS_CALC_VALUE_SIZE,
                   APPS_CALC_VALUE_STYLE);
}

static void _apps_calc_push(apps_calc_t *calc)
{
    for (int i = 0; i < APPS_CALC_STACK - 1; i++)
        calc->stack[i + 1] = calc->stack[i];
    calc->pending_delete = true;
}

static int _apps_calc_pop(apps_calc_t *calc)
{
    int v = calc->stack[0];
    for (int i = 0; i < APPS_CALC_STACK - 1; i++)
        calc->stack[i] = calc->stack[i + 1];
    return v;
}

static void _apps_calc_digit(apps_calc_t *calc, int digit)
{
    if (calc->pending_delete) {
        calc->stack[0] = 0;
        calc->pending_delete = false;
    }
    calc->stack[0] = calc->stack[0] * 10 + digit;
    _apps_calc_update_value(calc);
}

static void _apps_calc_button_signal(twin_button_t *button,
                                     twin_button_signal_t signal,
                                     void *closure)
{
    apps_calc_t *calc = closure;
    int a, b;

    if (signal != TwinButtonSignalDown)
        return;
    int i = _apps_calc_button_to_id(calc, button);
    if (i < 0)
        return;

    switch (i) {
#define _(x) APPS_CALC_##x
    case _(PLUS):
    case _(MINUS):
    case _(TIMES):
    case _(DIVIDE):
        calc->pending_op = i;
        _apps_calc_push(calc);
        break;
    case _(EQUAL):
        if (calc->pending_op != 0) {
            b = _apps_calc_pop(calc);
            a = _apps_calc_pop(calc);
            switch (calc->pending_op) {
            case _(PLUS):
                a = a + b;
                break;
            case _(MINUS):
                a = a - b;
                break;
            case _(TIMES):
                a = a * b;
                break;
            case _(DIVIDE):
                if (!b)
                    a = 0;
                else
                    a = a / b;
                break;
            }
            _apps_calc_push(calc);
            calc->stack[0] = a;
            _apps_calc_update_value(calc);
            calc->pending_op = 0;
        }
        calc->pending_delete = true;
        break;
    case _(CLEAR):
        for (i = 0; i < APPS_CALC_STACK; i++)
            calc->stack[i] = 0;
        calc->pending_op = 0;
        calc->pending_delete = true;
        _apps_calc_update_value(calc);
        break;
#undef _
    default:
        _apps_calc_digit(calc, i);
        break;
    }
}

void apps_calc_start(twin_screen_t *screen,
                     const char *name,
                     int x,
                     int y,
                     int w,
                     int h)
{
    apps_calc_t *calc = malloc(sizeof(apps_calc_t));

    calc->toplevel = twin_toplevel_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, name);
    calc->display =
        twin_label_create(&calc->toplevel->box, "0", APPS_CALC_VALUE_FG,
                          APPS_CALC_VALUE_SIZE, APPS_CALC_VALUE_STYLE);
    twin_widget_set(&calc->display->widget, APPS_CALC_VALUE_BG);
    calc->display->align = TwinAlignRight;
    calc->display->widget.shape = TwinShapeLozenge;
    calc->keys = twin_box_create(&calc->toplevel->box, TwinBoxHorz);
    for (int i = 0; i < APPS_CALC_COLS; i++) {
        calc->cols[i] = twin_box_create(calc->keys, TwinBoxVert);
        for (int j = 0; j < APPS_CALC_ROWS; j++) {
            int b = calc_layout[j][i];
            calc->buttons[b] = twin_button_create(
                calc->cols[i], apps_calc_labels[b], APPS_CALC_BUTTON_FG,
                APPS_CALC_BUTTON_SIZE, APPS_CALC_BUTTON_STYLE);
            twin_widget_set(&calc->buttons[b]->label.widget,
                            APPS_CALC_BUTTON_BG);
            calc->buttons[b]->signal = _apps_calc_button_signal;
            calc->buttons[b]->closure = calc;
            calc->buttons[b]->label.widget.shape = TwinShapeEllipse;
            if (i || j)
                calc->buttons[b]->label.widget.copy_geom =
                    &calc->buttons[calc_layout[0][0]]->label.widget;
        }
    }

    for (int i = 0; i < APPS_CALC_STACK; i++)
        calc->stack[i] = 0;
    calc->pending_delete = true;
    calc->pending_op = 0;
    twin_toplevel_show(calc->toplevel);
}
