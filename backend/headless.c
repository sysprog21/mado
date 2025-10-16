/*
 * Twin - A Tiny Window System
 * Copyright (c) 2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <twin.h>
#include <unistd.h>

#include "headless-shm.h"
#include "twin_private.h"

typedef struct {
    twin_headless_shm_t *shm;
    twin_argb32_t *framebuffer;  /* Points to current back buffer */
    twin_argb32_t *front_buffer; /* Buffer visible to control tool */
    twin_argb32_t *back_buffer;  /* Buffer for rendering */
    int shm_fd;
    size_t shm_size;
    uint32_t last_cmd_seq;
    uint64_t last_frame_time;
    uint32_t frame_count;
    bool running;
} twin_headless_t;

#define SCREEN(x) ((twin_context_t *) x)->screen
#define PRIV(x) ((twin_headless_t *) ((twin_context_t *) x)->priv)

/* Get current timestamp in microseconds */
static uint64_t _twin_headless_get_timestamp(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

/* Update event statistics in shared memory */
static void _twin_headless_track_event(twin_headless_t *tx,
                                       const char *event_type)
{
    if (!tx->shm)
        return;

    uint64_t now = _twin_headless_get_timestamp();
    tx->shm->last_activity_timestamp = now;
    tx->shm->events_processed_total++;

    /* Update per-second counter */
    uint64_t elapsed = now - tx->shm->events_second_timestamp;
    if (elapsed >= 1000000) { /* 1 second in microseconds */
        tx->shm->events_second_timestamp = now;
        tx->shm->events_processed_second = 0;
    }
    tx->shm->events_processed_second++;

    /* Track specific event types */
    if (strstr(event_type, "mouse"))
        tx->shm->mouse_events_total++;
}

static void _twin_headless_put_begin(twin_coord_t left,
                                     twin_coord_t top,
                                     twin_coord_t right,
                                     twin_coord_t bottom,
                                     void *closure)
{
    (void) left;
    (void) top;
    (void) right;
    (void) bottom;
    (void) closure;
}

static void _twin_headless_put_span(twin_coord_t left,
                                    twin_coord_t top,
                                    twin_coord_t right,
                                    twin_argb32_t *pixels,
                                    void *closure)
{
    twin_screen_t *screen = SCREEN(closure);
    twin_headless_t *tx = PRIV(closure);

    if (!tx->framebuffer)
        return;

    twin_coord_t width = screen->width;
    for (twin_coord_t x = left; x < right; x++) {
        tx->framebuffer[top * width + x] = *pixels++;
    }
}

static void _twin_headless_process_commands(twin_context_t *ctx)
{
    twin_headless_t *tx = ctx->priv;
    twin_headless_shm_t *shm = tx->shm;

    if (!shm || shm->command_seq == tx->last_cmd_seq)
        return;

    tx->last_cmd_seq = shm->command_seq;

    /* Track command processing */
    _twin_headless_track_event(tx, "command");
    tx->shm->commands_processed_total++;

    switch (shm->command) {
    case TWIN_HEADLESS_CMD_SCREENSHOT:
        /* Screenshot functionality moved to control tool */
        shm->response_status = 0;
        strcpy(shm->response_data, "Screenshot data available in framebuffer");
        break;

    case TWIN_HEADLESS_CMD_GET_STATE:
        shm->response_status = 0;
        snprintf(shm->response_data, sizeof(shm->response_data),
                 "events=%u frames=%u fps=%u running=%d",
                 shm->events_processed_total, shm->frames_rendered_total,
                 shm->frames_per_second, tx->running);
        break;

    case TWIN_HEADLESS_CMD_SHUTDOWN:
        tx->running = false;
        shm->response_status = 0;
        strcpy(shm->response_data, "Shutting down");
        break;

    default:
        shm->response_status = -1;
        strcpy(shm->response_data, "Unknown command");
        break;
    }

    shm->response_seq = shm->command_seq;
}

static void _twin_headless_inject_events(twin_context_t *ctx)
{
    twin_headless_t *tx = ctx->priv;
    twin_headless_shm_t *shm = tx->shm;
    twin_screen_t *screen = ctx->screen;

    if (!shm)
        return;

    while (shm->event_read_idx != shm->event_write_idx) {
        uint32_t idx = shm->event_read_idx % TWIN_HEADLESS_MAX_EVENTS;
        twin_headless_event_t *evt = &shm->events[idx];
        twin_event_t twin_evt;

        memset(&twin_evt, 0, sizeof(twin_evt));
        twin_evt.kind = evt->kind;

        switch (evt->kind) {
        case TwinEventButtonDown:
        case TwinEventButtonUp:
        case TwinEventMotion:
            twin_evt.u.pointer.screen_x = evt->u.pointer.x;
            twin_evt.u.pointer.screen_y = evt->u.pointer.y;
            twin_evt.u.pointer.button = evt->u.pointer.button;
            _twin_headless_track_event(tx, "mouse");
            break;

        default:
            _twin_headless_track_event(tx, "other");
            break;
        }

        twin_screen_dispatch(screen, &twin_evt);
        shm->event_read_idx++;
    }
}

static void twin_headless_update_damage(void *closure)
{
    twin_context_t *ctx = closure;
    twin_screen_t *screen = ctx->screen;
    twin_headless_t *tx = ctx->priv;

    if (!twin_screen_damaged(screen))
        return;

    twin_screen_update(screen);

    /* Flip buffers - copy back buffer to front buffer */
    size_t buffer_size = screen->width * screen->height * sizeof(twin_argb32_t);
    memcpy(tx->front_buffer, tx->back_buffer, buffer_size);

    /* Update frame statistics */
    uint64_t now = _twin_headless_get_timestamp();
    tx->frame_count++;

    /* Calculate FPS every second */
    uint64_t elapsed = now - tx->last_frame_time;
    if (elapsed >= 1000000) { /* 1 second in microseconds */
        if (tx->shm) {
            tx->shm->frames_per_second =
                (uint32_t) ((double) tx->frame_count * 1000000.0 / elapsed);
            tx->shm->frames_rendered_total += tx->frame_count;
        }
        tx->last_frame_time = now;
        tx->frame_count = 0;
    }
}

static void twin_headless_configure(twin_context_t *ctx)
{
    twin_screen_t *screen = ctx->screen;

    screen->put_begin = _twin_headless_put_begin;
    screen->put_span = _twin_headless_put_span;
}

static bool twin_headless_poll(twin_context_t *ctx)
{
    twin_headless_t *tx = ctx->priv;

    if (!tx->running)
        return false;

    /* Process commands from control tool */
    _twin_headless_process_commands(ctx);

    /* Inject events from control tool */
    _twin_headless_inject_events(ctx);

    /* Screen update is handled by damage callback, no need to call here */

    /* Update heartbeat timestamp periodically to show backend is alive */
    if (tx->shm) {
        uint64_t now = _twin_headless_get_timestamp();
        uint64_t elapsed = now - tx->shm->last_activity_timestamp;

        /* Update heartbeat every 500ms when idle to show backend is alive */
        if (elapsed >= 500000) /* 500ms in microseconds */
            tx->shm->last_activity_timestamp = now;
    }

    return tx->running;
}

static void twin_headless_exit(twin_context_t *ctx)
{
    if (!ctx)
        return;

    twin_headless_t *tx = ctx->priv;
    if (tx) {
        if (tx->shm)
            munmap(tx->shm, tx->shm_size);
        if (tx->shm_fd >= 0) {
            close(tx->shm_fd);
            shm_unlink(TWIN_HEADLESS_SHM_NAME);
        }
        free(tx);
    }
    free(ctx);
}

twin_context_t *twin_headless_init(int width, int height)
{
    twin_context_t *ctx = calloc(1, sizeof(twin_context_t));
    if (!ctx)
        return NULL;

    ctx->priv = calloc(1, sizeof(twin_headless_t));
    if (!ctx->priv) {
        free(ctx);
        return NULL;
    }

    twin_headless_t *tx = ctx->priv;
    tx->running = true;
    tx->shm_fd = -1;

    /* Calculate shared memory size (header + 2 buffers for double buffering) */
    size_t header_size = sizeof(twin_headless_shm_t);
    size_t fb_size = width * height * sizeof(twin_argb32_t);
    tx->shm_size = header_size + (fb_size * 2); /* Two buffers */

    /* Create shared memory */
    tx->shm_fd = shm_open(TWIN_HEADLESS_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (tx->shm_fd < 0) {
        fprintf(stderr, "Failed to create shared memory: %s\n",
                strerror(errno));
        goto error;
    }

    /* Ensure size is page-aligned for macOS */
    size_t page_size = getpagesize();
    tx->shm_size = ((tx->shm_size + page_size - 1) / page_size) * page_size;

    if (ftruncate(tx->shm_fd, tx->shm_size) < 0) {
        fprintf(stderr, "Failed to resize shared memory to %zu bytes: %s\n",
                tx->shm_size, strerror(errno));
        goto error;
    }

    /* Map shared memory */
    tx->shm = mmap(NULL, tx->shm_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   tx->shm_fd, 0);
    if (tx->shm == MAP_FAILED) {
        fprintf(stderr, "Failed to map shared memory: %s\n", strerror(errno));
        goto error;
    }

    /* Initialize shared memory header */
    memset(tx->shm, 0, tx->shm_size);
    tx->shm->magic = TWIN_HEADLESS_MAGIC;
    tx->shm->version = TWIN_HEADLESS_VERSION;
    tx->shm->width = width;
    tx->shm->height = height;

    /* Initialize event tracking fields */
    uint64_t now = _twin_headless_get_timestamp();
    tx->shm->last_activity_timestamp = now;
    tx->shm->events_second_timestamp = now;
    tx->shm->events_processed_total = 0;
    tx->shm->events_processed_second = 0;
    tx->shm->commands_processed_total = 0;
    tx->shm->mouse_events_total = 0;
    tx->shm->frames_per_second = 0;
    tx->shm->frames_rendered_total = 0;

    /* Initialize frame tracking */
    tx->last_frame_time = now;
    tx->frame_count = 0;

    /* Set framebuffer pointers */
    tx->front_buffer = (twin_argb32_t *) ((char *) tx->shm + header_size);
    tx->back_buffer = tx->front_buffer + (width * height);
    tx->framebuffer = tx->back_buffer; /* Start rendering to back buffer */

    /* Create screen */
    ctx->screen = twin_screen_create(width, height, _twin_headless_put_begin,
                                     _twin_headless_put_span, ctx);
    if (!ctx->screen)
        goto error;

    twin_screen_register_damaged(ctx->screen, twin_headless_update_damage, ctx);

    return ctx;

error:
    if (tx->shm && tx->shm != MAP_FAILED)
        munmap(tx->shm, tx->shm_size);
    if (tx->shm_fd >= 0) {
        close(tx->shm_fd);
        shm_unlink(TWIN_HEADLESS_SHM_NAME);
    }
    free(ctx->priv);
    free(ctx);
    return NULL;
}

/* Register the headless backend */
const twin_backend_t g_twin_backend = {
    .init = twin_headless_init,
    .configure = twin_headless_configure,
    .poll = twin_headless_poll,
    .exit = twin_headless_exit,
};
