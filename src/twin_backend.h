/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#ifndef _TWIN_BACKEND_H_
#define _TWIN_BACKEND_H_

#include <stdbool.h>

#include "twin_private.h"

typedef struct twin_backend {
    /* Initialize the backend */
    twin_context_t *(*init)(int width, int height);

    /* Configure the device */
    /* FIXME: Refine the parameter list */
    void (*configure)(twin_context_t *ctx);

    /* Device cleanup when drawing is done */
    void (*exit)(twin_context_t *ctx);
} twin_backend_t;

#endif /* _TWIN_BACKEND_H_ */
