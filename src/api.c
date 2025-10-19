/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <assert.h>
#include <twin.h>

#include "twin_private.h"

extern twin_backend_t g_twin_backend;

/**
 * Create a new Twin context with specified dimensions.
 *
 * Initializes the backend and creates a screen context for rendering.
 *
 * @width  : Screen width in pixels
 * @height : Screen height in pixels
 * @return : Twin context, or NULL on failure
 */
twin_context_t *twin_create(int width, int height)
{
    assert(g_twin_backend.init && "Backend not registered");

    twin_context_t *ctx = g_twin_backend.init(width, height);
    if (!ctx) {
#ifdef CONFIG_LOGGING
        log_error("Failed to initialize Twin context (%dx%d)", width, height);
#endif
    }
    return ctx;
}

/**
 * Destroy a Twin context and release all resources.
 *
 * Cleans up backend resources, frees memory, and terminates the context.
 *
 * @ctx : Twin context to destroy
 */
void twin_destroy(twin_context_t *ctx)
{
    assert(g_twin_backend.exit && "Backend not registered");

    if (ctx)
        g_twin_backend.exit(ctx);
}

/**
 * Start the Twin application with initialization callback.
 *
 * This function provides a platform-independent way to start Twin applications.
 * It delegates to the backend's start function, which handles platform-specific
 * main loop setup.
 *
 * @ctx           : Twin context to run
 * @init_callback : Application initialization function (called once before
 * event loop)
 */
void twin_run(twin_context_t *ctx, void (*init_callback)(twin_context_t *))
{
    assert(ctx && "NULL context passed to twin_run");
    assert(g_twin_backend.start && "Backend start function not registered");

    g_twin_backend.start(ctx, init_callback);
}
