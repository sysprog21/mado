/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <twin.h>

#include "twin_private.h"

twin_context_t *twin_create(int width, int height)
{
    /* Initialize the memory pool before any allocations occur. */
    twin_mem_pool_init();

    twin_context_t *ctx = g_twin_backend.init(width, height);
    if (!ctx) {
        log_error("Backend initialization failed (%dx%d)", width, height);
        return NULL;
    }
#if defined(CONFIG_CURSOR)
    /* Backends that draw no cursor of their own get the arrow */
    twin_pixmap_t *cursor = _twin_make_cursor();
    if (cursor)
        twin_screen_set_cursor(ctx->screen, cursor, 0, 0);
#endif
    return ctx;
}

void twin_destroy(twin_context_t *ctx)
{
    if (!ctx)
        return;

    /* Screen teardown may emit damage callbacks through backend-owned closure.
     */
    if (ctx->screen) {
        twin_screen_destroy(ctx->screen);
        ctx->screen = NULL;
    }

    /* Backend frees its private data and ctx */
    g_twin_backend.exit(ctx);

    /* Cached coverage outlives screens; release it with the system */
    twin_cache_flush();

    /* Report memory statistics after all teardown */
    twin_memory_info_t mem;
    twin_memory_get_info(&mem);
    if (mem.peak_bytes)
        log_info(
            "Memory: current %zu bytes, peak %zu bytes, "
            "allocs %zu, frees %zu",
            mem.current_bytes, mem.peak_bytes, mem.total_allocs,
            mem.total_frees);
}

bool twin_dispatch_once(twin_context_t *ctx)
{
    _twin_run_timeout();
    _twin_run_work();
    /* The backend sleeps here until input or the next thing to do */
    return g_twin_backend.poll(ctx);
}

void twin_run(twin_context_t *ctx, void (*init_callback)(twin_screen_t *))
{
    if (!ctx)
        return;
    if (g_twin_backend.start) {
        g_twin_backend.start(ctx, init_callback);
        return;
    }
    if (init_callback)
        init_callback(ctx->screen);
    while (twin_dispatch_once(ctx))
        ;
}
