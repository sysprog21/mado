/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <twin.h>
#include <unistd.h>

#include "linux_input.h"

#define MOUSE_EVENT_LEFT_BTN 0x1
#define MOUSE_EVENT_RIGHT_BTN 0x2
#define MOUSE_EVENT_DATA_SIZE 3

#define MOUSE_DEV_NAME "MOUSE"
#define MOUSE_DEV_DEFAULT "/dev/input/mice"

typedef struct {
    twin_screen_t *screen;
    int fd;
    char residual[2];
    int res_cnt;
    int btns;
    int x, y;
} twin_linux_input_t;

static void _mouse_check_bounds(twin_linux_input_t *tm)
{
    if (tm->x < 0)
        tm->x = 0;
    if (tm->x > tm->screen->width)
        tm->x = tm->screen->width;
    if (tm->y < 0)
        tm->y = 0;
    if (tm->y > tm->screen->height)
        tm->y = tm->screen->height;
}

static bool twin_linux_input_events(int file, twin_file_op_t ops, void *closure)
{
    twin_linux_input_t *tm = closure;
    char evts[34]; /* XXX: How big should we allocate for the event buffer? */
    int n = tm->res_cnt;
    twin_event_t tev;

    /* Prepare input data for unpacking */
    if (n)
        memcpy(evts, tm->residual, n);
    n += read(file, evts + n, 32); /* XXX: Same as above */

    char *ep;
    for (ep = evts; n >= MOUSE_EVENT_DATA_SIZE;
         n -= MOUSE_EVENT_DATA_SIZE, ep += MOUSE_EVENT_DATA_SIZE) {
        /* Add incremental change to the cursor position */
        int dx = +ep[1];
        int dy = -ep[2];
        if (dx || dy) {
            tm->x += dx, tm->y += dy;
            _mouse_check_bounds(tm);
            tev.kind = TwinEventMotion;
            tev.u.pointer.screen_x = tm->x;
            tev.u.pointer.screen_y = tm->y;
            tev.u.pointer.button = tm->btns;
            twin_screen_dispatch(tm->screen, &tev);
        }

        /* Handle left mouse button */
        int btn = ep[0] & MOUSE_EVENT_LEFT_BTN;
        if (btn != tm->btns) {
            tm->btns = btn;
            tev.kind = btn ? TwinEventButtonDown : TwinEventButtonUp;
            tev.u.pointer.screen_x = tm->x;
            tev.u.pointer.screen_y = tm->y;
            tev.u.pointer.button = tm->btns;
            twin_screen_dispatch(tm->screen, &tev);
        }
    }

    /* Preserve left unpacked data */
    tm->res_cnt = n;
    if (n)
        memcpy(tm->residual, ep, n);

    return true;
}

void *twin_linux_input_create(twin_screen_t *screen)
{
    /* Read mouse device file path from environment variable */
    char *mouse_dev_path = getenv(MOUSE_DEV_NAME);
    if (!mouse_dev_path) {
        printf("Environment variable $MOUSE not set, use %s by default\n",
               MOUSE_DEV_DEFAULT);
        mouse_dev_path = MOUSE_DEV_DEFAULT;
    }

    /* Create object for handling Linux input system */
    twin_linux_input_t *tm = calloc(1, sizeof(twin_linux_input_t));
    if (!tm)
        return NULL;

    tm->screen = screen;

    /* Centering the cursor position */
    tm->x = screen->width / 2;
    tm->y = screen->height / 2;

    /* Open the mouse device file */
    tm->fd = open(mouse_dev_path, O_RDONLY);
    if (tm->fd < 0) {
        free(tm);
        return NULL;
    }

    /* Set file handler for reading input device file */
    twin_set_file(twin_linux_input_events, tm->fd, TWIN_READ, tm);

    return tm;
}

void twin_linux_input_destroy(void *_tm)
{
    twin_linux_input_t *tm = _tm;
    close(tm->fd);
    free(tm);
}
