/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Headless test harness
 *
 * Runs a screen entirely in-process: scan-out lands in a memory framebuffer,
 * time is a virtual clock advanced by the test, and input is injected
 * through twin_screen_dispatch().  Mado is integer-only, so with the built-in
 * renderer every frame is bit-exact across hosts; tests can compare CRCs.
 */

#ifndef _TWIN_TEST_HARNESS_H_
#define _TWIN_TEST_HARNESS_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <twin.h>

#include "twin_private.h"

typedef struct {
    twin_screen_t *screen;
    twin_argb32_t *fb; /* width * height, written by scan-out */
    twin_coord_t width, height;
} th_t;

/* Create a screen of the given size under the given window manager */
th_t *th_create(twin_coord_t width, twin_coord_t height, twin_wm_mode_t wm);
void th_destroy(th_t *t);

/* Resize the screen (rotation, host window resize) and its framebuffer */
void th_resize(th_t *t, twin_coord_t width, twin_coord_t height);

/* Run timeouts and work, then scan out pending damage (one frame) */
void th_frame(th_t *t);

/* Advance virtual time by @ms in 16 ms frames */
void th_advance(th_t *t, int ms);

/* Run frames (advancing time) until no timeout is due within @max_ms */
void th_settle(th_t *t, int max_ms);

/* Pointer input in screen coordinates */
void th_press(th_t *t, twin_coord_t x, twin_coord_t y);
void th_move(th_t *t, twin_coord_t x, twin_coord_t y);
void th_release(th_t *t, twin_coord_t x, twin_coord_t y);
void th_tap(th_t *t, twin_coord_t x, twin_coord_t y);
void th_drag(th_t *t,
             twin_coord_t x0,
             twin_coord_t y0,
             twin_coord_t x1,
             twin_coord_t y1,
             int steps);
void th_key(th_t *t, twin_ucs4_t ucs4);

/* CRC-32 of the framebuffer */
uint32_t th_crc(const th_t *t);

/* Pixel at (x, y) */
twin_argb32_t th_pixel(const th_t *t, twin_coord_t x, twin_coord_t y);

/*
 * Damage consistency: recomposite the whole screen from scratch with every
 * optimization off and compare with the incrementally maintained frame.
 * A mismatch means some change was not damaged (stale pixels on screen).
 * Returns the number of differing pixels.
 */
int th_damage_check(th_t *t);

/* Heap minus bounded reuse pools (coverage cache, path/mask/span caches) */
size_t th_heap_net(const th_t *t);
/* The whole heap, pools included: what the allocator's peak is measured
 * against.  Without MEMORY_STATS, th_heap_net() reads 0 (with a one-time
 * notice) so leak checks compare 0 with 0 instead of underflowing. */
size_t th_heap_raw(void);

/*
 * Minimal check framework
 */
extern int th_checks, th_failures;

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        th_checks++;                                                        \
        if (!(cond)) {                                                      \
            th_failures++;                                                  \
            fprintf(stderr, "FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); \
            fprintf(stderr, __VA_ARGS__);                                   \
            fputc('\n', stderr);                                            \
        }                                                                   \
    } while (0)

/* Evaluates each operand once (they may have side effects).  The
 * temporaries carry a prefix no test uses, so they cannot shadow an
 * identifier an operand names. */
#define CHECK_EQ(a, b, what)                                              \
    do {                                                                  \
        long long th_eq_a_ = (long long) (a), th_eq_b_ = (long long) (b); \
        CHECK(th_eq_a_ == th_eq_b_, "%s: got %lld, expected %lld", what,  \
              th_eq_a_, th_eq_b_);                                        \
    } while (0)

/* NUL-terminated strings; @a may be NULL */
#define CHECK_STR(a, b, what)                               \
    do {                                                    \
        const char *th_str_a_ = (a), *th_str_b_ = (b);      \
        CHECK(th_str_a_ && !strcmp(th_str_a_, th_str_b_),   \
              "%s: got \"%s\", expected \"%s\"", what,      \
              th_str_a_ ? th_str_a_ : "(null)", th_str_b_); \
    } while (0)

/* Print the summary line and return the process exit status */
int th_report(const char *suite);

/* Golden checksums: tests/golden/<suite>.txt holds "name crc" lines */
void th_golden(const char *suite,
               const char *name,
               uint32_t crc,
               const th_t *t);

#endif /* _TWIN_TEST_HARNESS_H_ */
