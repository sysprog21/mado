/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twin_private.h"

void twin_dispatch(void)
{
    for (;;) {
        _twin_run_timeout();
        _twin_run_work();
        if (!_twin_run_file(_twin_timeout_delay()))
            break;
    }
}
