/*
 * Twin - A Tiny Window System
 * Copyright (c) 2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#ifndef _TWIN_HEADLESS_SHM_H_
#define _TWIN_HEADLESS_SHM_H_

#include <stddef.h>
#include <stdint.h>
#include <twin.h>

/* Shared memory layout definitions - shared between backend and control tool */

#define TWIN_HEADLESS_SHM_NAME "/mado-headless-shm"
#define TWIN_HEADLESS_MAGIC 0x5457494E /* "TWIN" */
#define TWIN_HEADLESS_VERSION 2
#define TWIN_HEADLESS_MAX_EVENTS 64

typedef enum {
    TWIN_HEADLESS_CMD_NONE = 0,
    TWIN_HEADLESS_CMD_SCREENSHOT,
    TWIN_HEADLESS_CMD_GET_STATE,
    TWIN_HEADLESS_CMD_INJECT_MOUSE,
    TWIN_HEADLESS_CMD_SHUTDOWN,
} twin_headless_cmd_t;

typedef struct {
    twin_event_kind_t kind;
    union {
        struct {
            twin_coord_t x, y;
            twin_count_t button;
        } pointer;
        struct {
            twin_ucs4_t ucs4;
        } ucs4;
    } u;
} twin_headless_event_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    /* Backend process: tells a live object from one a crash left behind */
    int32_t owner_pid;

    /* Screen info */
    twin_coord_t width, height;

    /* Event processing statistics */
    uint64_t last_activity_timestamp;  /* Unix timestamp in microseconds */
    uint32_t events_processed_total;   /* Total events processed since start */
    uint32_t events_processed_second;  /* Events processed in current second */
    uint64_t events_second_timestamp;  /* Timestamp for current second period */
    uint32_t commands_processed_total; /* Total commands from control tool */
    uint32_t mouse_events_total;       /* Total mouse events processed */

    /* Rendering statistics */
    uint32_t frames_per_second;     /* Current FPS */
    uint32_t frames_rendered_total; /* Total frames rendered since start */

    /* Command interface */
    twin_headless_cmd_t command;
    uint32_t command_seq;
    char command_data[256];

    /* Response */
    uint32_t response_seq;
    int32_t response_status;
    char response_data[256];

    /* Event injection */
    uint32_t event_write_idx, event_read_idx;
    twin_headless_event_t events[TWIN_HEADLESS_MAX_EVENTS];

    /* Framebuffer follows this header */
    /* twin_argb32_t framebuffer[width * height]; */
} twin_headless_shm_t;

/*
 * Bytes for the header plus @frames frames of @width x @height ARGB32, or 0
 * when that overflows.  The backend sizes the object with it and clients
 * check what they mapped against it, so both agree on the layout.
 */
static inline size_t twin_headless_shm_bytes(size_t width,
                                             size_t height,
                                             size_t frames)
{
    size_t n;
    if (__builtin_mul_overflow(width, height, &n) ||
        __builtin_mul_overflow(n, frames * sizeof(twin_argb32_t), &n) ||
        __builtin_add_overflow(n, sizeof(twin_headless_shm_t), &n))
        return 0;
    return n;
}

#endif /* _TWIN_HEADLESS_SHM_H_ */
