/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <assert.h>
#include <twin.h>

#include "twin_private.h"

/* FIXME: Refine API design */

extern twin_backend_t g_twin_backend;

twin_context_t *twin_create(int width, int height)
{
    return g_twin_backend.init(width, height);
}

void twin_destroy(twin_context_t *ctx)
{
    return g_twin_backend.exit(ctx);
}
