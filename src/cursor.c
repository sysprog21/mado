/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/* Manipulating cursor images */

#include <stddef.h>
#include <twin.h>

/* FIXME: Improve the default cursor instead of using a minimalist dot. */
static uint32_t _twin_cursor_default[] = {
    0x00000000, 0x88ffffff, 0x88ffffff, 0x00000000, 0x88ffffff, 0xff000000,
    0xff000000, 0x88ffffff, 0x88ffffff, 0xff000000, 0xff000000, 0x88ffffff,
    0x00000000, 0x88ffffff, 0x88ffffff, 0x00000000,
};

twin_pixmap_t *twin_make_cursor(int *hx, int *hy)
{
    twin_pointer_t data = {.argb32 = _twin_cursor_default};
    twin_pixmap_t *cur = twin_pixmap_create_const(TWIN_ARGB32, 4, 4, 16, data);
    if (!cur)
        return NULL;
    *hx = *hy = 4;
    return cur;
}
