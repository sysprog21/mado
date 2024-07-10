/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twin_private.h"

#define G(d) ((signed char) ((d) * 64))

#define ICON_THIN (1.0 / 20.0)

#define L(v) G(v + ICON_THIN / 2)
#define T(v) G(v + ICON_THIN / 2)
#define R(v) G(v - ICON_THIN / 2)
#define B(v) G(v - ICON_THIN / 2)

/* clang-format off */
static const signed char _twin_itable[] = {
    /* Menu */
#define TWIN_MENU_POS	    0
    'm', L(0), T(0),
    'd', R(1), T(0),
    'd', R(1), B(1),
    'd', L(0), B(1),
    'x',
    's',
    'm', G(0.2), G(0.2),
    'd', G(0.8), G(0.2),
    's',
    'm', G(0.2), G(0.4),
    'd', G(0.8), G(0.4),
    's',
    'm', G(0.2), G(0.6),
    'd', G(0.8), G(0.6),
    's',
    'm', G(0.2), G(0.8),
    'd', G(0.8), G(0.8),
    's',
    'e',
#define TWIN_MENU_LEN	    43
    /* Minimize */
#define TWIN_MINIMIZE_POS   TWIN_MENU_POS + TWIN_MENU_LEN
    'm', L(0), G(0.8),
    'd', L(0), B(1),
    'd', R(1), B(1),
    'd', R(1), G(0.8),
    'x',
    'w', G(0.05),
    'p',
    'e',
#define TWIN_MINIMIZE_LEN   17
    /* Maximize */
#define TWIN_MAXIMIZE_POS   TWIN_MINIMIZE_POS + TWIN_MINIMIZE_LEN
    'm', L(0), T(0),
    'd', L(0), G(0.2),
    'd', R(1), G(0.2),
    'd', R(1), T(0),
    'f',
    'm', L(0), T(0),
    'd', L(0), B(1),
    'd', R(1), B(1),
    'd', R(1), T(0),
    'x',
    's',
    'e',
#define TWIN_MAXIMIZE_LEN   28
    /* Close */
#define TWIN_CLOSE_POS	    TWIN_MAXIMIZE_POS + TWIN_MAXIMIZE_LEN
    'm', L(0), T(0),
    'd', L(0), T(0.1),
    'd', G(0.4), G(0.5),
    'd', L(0), B(0.9),
    'd', L(0), B(1),
    'd', L(0.1), B(1),
    'd', G(0.5), G(0.6),
    'd', R(0.9), B(1),
    'd', R(1), B(1),
    'd', R(1), B(0.9),
    'd', G(0.6), G(0.5),
    'd', R(1), T(0.1),
    'd', R(1), T(0),
    'd', R(0.9), T(0),
    'd', G(0.5), G(0.4),
    'd', L(0.1), T(0),
    'x',
    'p',
    'e',
#define TWIN_CLOSE_LEN	    51
    /* Resize */
#define TWIN_RESIZE_POS	    TWIN_CLOSE_POS + TWIN_CLOSE_LEN
    'm', L(0), G(-0.8),
    'd', L(0), T(0),
    'd', G(-0.8), T(0),
    'd', G(-0.8), G(0.2),
    'd', G(0.2), G(0.2),
    'd', G(0.2), G(-0.8),
    'x',
    'p',
    'e',
#define TWIN_RESIZE_LEN	    21
};
/* clang-format on */

const uint16_t _twin_icons[] = {
    TWIN_MENU_POS,  TWIN_MINIMIZE_POS, TWIN_MAXIMIZE_POS,
    TWIN_CLOSE_POS, TWIN_RESIZE_POS,
};

#define V(i) (g[i] << 10)

#define TWIN_ICON_FILL 0xff808080
#define TWIN_ICON_STROKE 0xff202020

void twin_icon_draw(twin_pixmap_t *pixmap,
                    twin_icon_t icon,
                    twin_matrix_t matrix)
{
    twin_path_t *path = twin_path_create();
    const signed char *g = _twin_itable + _twin_icons[icon];
    twin_fixed_t stroke_width = twin_double_to_fixed(ICON_THIN);

    twin_path_set_matrix(path, matrix);
    for (;;) {
        switch (*g++) {
        case 'm':
            twin_path_move(path, V(0), V(1));
            g += 2;
            continue;
        case 'd':
            twin_path_draw(path, V(0), V(1));
            g += 2;
            continue;
        case 'c':
            twin_path_curve(path, V(0), V(1), V(2), V(3), V(4), V(5));
            g += 6;
            continue;
        case 'x':
            twin_path_close(path);
            continue;
        case 'w':
            stroke_width = V(0);
            g += 1;
            continue;
        case 'f':
            twin_paint_path(pixmap, TWIN_ICON_FILL, path);
            twin_path_empty(path);
            continue;
        case 's':
            twin_paint_stroke(pixmap, TWIN_ICON_STROKE, path, stroke_width);
            twin_path_empty(path);
            continue;
        case 'p':
            twin_paint_path(pixmap, TWIN_ICON_FILL, path);
            twin_paint_stroke(pixmap, TWIN_ICON_STROKE, path, stroke_width);
            twin_path_empty(path);
            continue;
        case 'e':
            break;
        }
        break;
    }
    twin_path_destroy(path);
}
