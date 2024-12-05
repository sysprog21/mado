/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <unistd.h>

#include "twin_backend.h"
#include "twin_private.h"

extern twin_backend_t g_twin_backend;

void twin_dispatch(twin_context_t *ctx)
{
    for (;;) {
        _twin_run_timeout();
        _twin_run_work();
        if (g_twin_backend.poll && !g_twin_backend.poll(ctx)) {
            twin_time_t delay = _twin_timeout_delay();
            if (delay > 0)
                usleep(delay * 1000);
            break;
        }
    }
}
