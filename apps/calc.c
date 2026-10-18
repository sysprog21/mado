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
#define APPS_CALC_VALUE_STYLE TwinStyleRoman
#define APPS_CALC_VALUE_FG 0xff000000
#define APPS_CALC_VALUE_BG 0x80808080
#define APPS_CALC_BUTTON_SIZE twin_int_to_fixed(15)
#define APPS_CALC_BUTTON_STYLE TwinStyleBold
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

    /* When the length reaches 9 digits, pressing the digit button will not
     * increase the number of digits. */
    if (calc->stack[0] > 99999999)
        return;

    calc->stack[0] = calc->stack[0] * 10 + digit;
    _apps_calc_update_value(calc);
}

static twin_dispatch_result_t _apps_calc_button_clicked(twin_widget_t *widget,
                                                        twin_event_t *event,
                                                        void *data)
{
    if (event->kind != TwinEventButtonSignalUp)
        return TwinDispatchContinue;

    apps_calc_t *calc = data;
    twin_button_t *button = (twin_button_t *) widget;
    int a, b;

    int i = _apps_calc_button_to_id(calc, button);
    if (i < 0)
        return TwinDispatchContinue;

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
    return TwinDispatchDone;
}

/* The display is destroyed with the window: release the calculator then */
static twin_dispatch_result_t _apps_calc_display_event(twin_widget_t *widget,
                                                       twin_event_t *event,
                                                       void *data)
{
    (void) widget;
    if (event->kind == TwinEventDestroy)
        free(data);
    return TwinDispatchContinue;
}

bool apps_calc_start(twin_screen_t *screen,
                     const char *name,
                     int x,
                     int y,
                     int w,
                     int h)
{
    apps_calc_t *calc = malloc(sizeof(apps_calc_t));
    if (!calc)
        return false;

    calc->toplevel = twin_toplevel_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h, name);
    if (!calc->toplevel) {
        free(calc);
        return false;
    }
    twin_box_t *top = twin_toplevel_box(calc->toplevel);
    calc->display =
        twin_label_create(top, "0", APPS_CALC_VALUE_FG, APPS_CALC_VALUE_SIZE,
                          APPS_CALC_VALUE_STYLE);
    if (!calc->display)
        goto fail;
    twin_widget_t *display = twin_label_widget(calc->display);
    twin_widget_set(display, APPS_CALC_VALUE_BG);
    twin_label_set_align(calc->display, TwinAlignRight);
    twin_widget_set_shape(display, TwinShapeLozenge);
    calc->keys = twin_box_create(top, TwinBoxHorz);
    if (!calc->keys)
        goto fail;
    for (int i = 0; i < APPS_CALC_COLS; i++) {
        calc->cols[i] = twin_box_create(calc->keys, TwinBoxVert);
        if (!calc->cols[i])
            goto fail;
        for (int j = 0; j < APPS_CALC_ROWS; j++) {
            int b = calc_layout[j][i];
            calc->buttons[b] = twin_button_create(
                calc->cols[i], apps_calc_labels[b], APPS_CALC_BUTTON_FG,
                APPS_CALC_BUTTON_SIZE, APPS_CALC_BUTTON_STYLE);
            if (!calc->buttons[b])
                goto fail;
            twin_widget_t *key = twin_button_widget(calc->buttons[b]);
            twin_widget_set(key, APPS_CALC_BUTTON_BG);
            if (!twin_widget_set_callback(key, _apps_calc_button_clicked, calc))
                goto fail;
            twin_widget_set_shape(key, TwinShapeEllipse);
            if (i || j)
                twin_widget_match_size(
                    key, twin_button_widget(calc->buttons[calc_layout[0][0]]));
        }
    }

    /* Last: from here on the display frees the calculator with the window */
    if (!twin_widget_set_callback(display, _apps_calc_display_event, calc))
        goto fail;

    for (int i = 0; i < APPS_CALC_STACK; i++)
        calc->stack[i] = 0;
    calc->pending_delete = true;
    calc->pending_op = 0;
    twin_toplevel_show(calc->toplevel);
    return true;

fail:
    twin_toplevel_destroy(calc->toplevel);
    free(calc);
    return false;
}
