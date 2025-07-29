/*
 * Twin - A Tiny Window System
 * Copyright (c) 2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "twin.h"

#define TEST_PIX_WIDTH 1200
#define TEST_PIX_HEIGHT 800
#define MIN_TEST_REPS 40
#define TARGET_TEST_TIME 500000 /* 0.5 seconds in microseconds */

/* Maximum repetitions to prevent excessive runtime */
#define MAX_REPS 2000000
/* Size-based calibration limits */
#define MAX_REPS_LARGE 20000   /* For operations >= 100x100 */
#define MAX_REPS_MEDIUM 200000 /* For operations >= 10x10 */

static twin_pixmap_t *src32, *dst32, *mask8;
static int test_width, test_height;

/* Sync time adjustment calculation */
static double sync_time_adjustment = 0.0;

static void test_argb32_source_argb32(void)
{
    twin_operand_t srco = {.source_kind = TWIN_PIXMAP, .u.pixmap = src32};
    twin_composite(dst32, 0, 0, &srco, 0, 0, NULL, 0, 0, TWIN_SOURCE,
                   test_width, test_height);
}

static void test_argb32_over_argb32(void)
{
    twin_operand_t srco = {.source_kind = TWIN_PIXMAP, .u.pixmap = src32};
    twin_composite(dst32, 0, 0, &srco, 0, 0, NULL, 0, 0, TWIN_OVER, test_width,
                   test_height);
}

static void test_argb32_over_argb32_mask(void)
{
    twin_operand_t srco = {.source_kind = TWIN_PIXMAP, .u.pixmap = src32};
    twin_operand_t masko = {.source_kind = TWIN_PIXMAP, .u.pixmap = mask8};
    twin_composite(dst32, 0, 0, &srco, 0, 0, &masko, 0, 0, TWIN_OVER,
                   test_width, test_height);
}

static void test_solid_source_argb32(void)
{
    twin_operand_t srco = {.source_kind = TWIN_SOLID, .u.argb = 0x80ff0000};
    twin_composite(dst32, 0, 0, &srco, 0, 0, NULL, 0, 0, TWIN_SOURCE,
                   test_width, test_height);
}

static void test_solid_over_argb32(void)
{
    twin_operand_t srco = {.source_kind = TWIN_SOLID, .u.argb = 0x80ff0000};
    twin_composite(dst32, 0, 0, &srco, 0, 0, NULL, 0, 0, TWIN_OVER, test_width,
                   test_height);
}

static void test_solid_over_argb32_mask(void)
{
    twin_operand_t srco = {.source_kind = TWIN_SOLID, .u.argb = 0x80ff0000};
    twin_operand_t masko = {.source_kind = TWIN_PIXMAP, .u.pixmap = mask8};
    twin_composite(dst32, 0, 0, &srco, 0, 0, &masko, 0, 0, TWIN_OVER,
                   test_width, test_height);
}

/* Test with different alpha levels */
static void test_solid_over_argb32_25(void)
{
    twin_operand_t srco = {.source_kind = TWIN_SOLID, .u.argb = 0x40ff0000};
    twin_composite(dst32, 0, 0, &srco, 0, 0, NULL, 0, 0, TWIN_OVER, test_width,
                   test_height);
}

static void test_solid_over_argb32_50(void)
{
    twin_operand_t srco = {.source_kind = TWIN_SOLID, .u.argb = 0x80ff0000};
    twin_composite(dst32, 0, 0, &srco, 0, 0, NULL, 0, 0, TWIN_OVER, test_width,
                   test_height);
}

static void test_solid_over_argb32_75(void)
{
    twin_operand_t srco = {.source_kind = TWIN_SOLID, .u.argb = 0xc0ff0000};
    twin_composite(dst32, 0, 0, &srco, 0, 0, NULL, 0, 0, TWIN_OVER, test_width,
                   test_height);
}

static void test_solid_over_argb32_opaque(void)
{
    twin_operand_t srco = {.source_kind = TWIN_SOLID, .u.argb = 0xffff0000};
    twin_composite(dst32, 0, 0, &srco, 0, 0, NULL, 0, 0, TWIN_OVER, test_width,
                   test_height);
}

/* Measure sync time by calling gettimeofday() repeatedly */
static void measure_sync_time(void)
{
    struct timeval start, end;
    int reps = 10000;

    gettimeofday(&start, NULL);
    for (int i = 0; i < reps; i++)
        gettimeofday(&end, NULL);
    gettimeofday(&end, NULL);

    uint64_t start_us = (uint64_t) start.tv_sec * 1000000U + start.tv_usec;
    uint64_t end_us = (uint64_t) end.tv_sec * 1000000U + end.tv_usec;

    sync_time_adjustment = ((double) (end_us - start_us)) / reps / 1000.0;
}

/* Calibrate test repetitions to run for approximately TARGET_TEST_TIME */
static int calibrate_reps(void (*test_func)(void))
{
    struct timeval start, end;
    int reps = 1000; /* Start with a reasonable base */
    uint64_t elapsed_us;
    int area = test_width * test_height;
    int max_reps_for_size;
    uint64_t target_time_for_size;

    /* Adjust limits based on operation size */
    if (area >= 10000) { /* 100x100 or larger */
        max_reps_for_size = MAX_REPS_LARGE;
        target_time_for_size = TARGET_TEST_TIME / 2; /* 250ms for large ops */
        reps = 200;           /* Start smaller for large operations */
    } else if (area >= 100) { /* 10x10 or larger */
        max_reps_for_size = MAX_REPS_MEDIUM;
        target_time_for_size =
            TARGET_TEST_TIME * 3 / 4; /* 375ms for medium ops */
        reps = 1000;
    } else { /* Small operations (1x1, etc.) */
        max_reps_for_size = MAX_REPS;
        target_time_for_size = TARGET_TEST_TIME; /* Full 500ms for tiny ops */
        reps = 2000;
    }

    /* Start with a reasonable number and scale up gradually */
    do {
        gettimeofday(&start, NULL);
        for (int i = 0; i < reps; i++)
            test_func();
        gettimeofday(&end, NULL);

        elapsed_us = ((uint64_t) end.tv_sec * 1000000U + end.tv_usec) -
                     ((uint64_t) start.tv_sec * 1000000U + start.tv_usec);

        /* If too fast, increase reps but cap at size-appropriate MAX_REPS */
        if (elapsed_us < target_time_for_size / 4 &&
            reps < max_reps_for_size / 4) {
            reps *= 4;
        } else if (elapsed_us < target_time_for_size / 2 &&
                   reps < max_reps_for_size / 2) {
            reps *= 2;
        } else {
            break;
        }
    } while (reps < max_reps_for_size);

    /* Calculate final repetitions for target time, but respect size limits */
    if (elapsed_us > 0 && reps < max_reps_for_size) {
        int target_reps =
            (int) ((double) reps * target_time_for_size / elapsed_us);
        reps =
            (target_reps > max_reps_for_size) ? max_reps_for_size : target_reps;
    }

    return reps > 0 ? (area >= 10000 ? 200 : area >= 100 ? 1000 : 2000) : reps;
}

static void run_test_series(const char *test_name,
                            void (*test_func)(void),
                            int width,
                            int height)
{
    test_width = width;
    test_height = height;

    /* Calibrate repetitions */
    int reps = calibrate_reps(test_func);

    /* Run multiple test iterations */
    double total_rate = 0.0;
    int num_runs = MIN_TEST_REPS;

    for (int run = 0; run < num_runs; run++) {
        struct timeval start, end;

        gettimeofday(&start, NULL);
        for (int i = 0; i < reps; i++)
            test_func();
        gettimeofday(&end, NULL);

        uint64_t start_us = (uint64_t) start.tv_sec * 1000000U + start.tv_usec;
        uint64_t end_us = (uint64_t) end.tv_sec * 1000000U + end.tv_usec;

        double elapsed_ms =
            ((double) (end_us - start_us)) / 1000.0 - sync_time_adjustment;
        double rate = (elapsed_ms > 0.0) ? (reps * 1000.0) / elapsed_ms : 0.0;

        printf("   %8d reps @ %8.4f msec (%12.1f/sec): %s\n", reps, elapsed_ms,
               rate, test_name);

        total_rate += rate;
    }

    /* Print summary */
    uint64_t total_reps = (uint64_t) reps * num_runs;
    double avg_rate = total_rate / num_runs;
    /* Calculate actual average time from the total rate */
    double avg_time = (avg_rate > 0.0) ? (total_reps * 1000.0) / avg_rate : 0.0;

    printf("%11" PRIu64 " trep @ %8.4f msec (%12.1f/sec): %s\n", total_reps,
           avg_time, avg_rate, test_name);
    printf("\n");
}

static void run_basic_tests(void)
{
    /* Basic pixmap composition tests */
    run_test_series("1x1 argb32 source", test_argb32_source_argb32, 1, 1);
    run_test_series("1x1 argb32 over", test_argb32_over_argb32, 1, 1);
    run_test_series("1x1 argb32 over mask", test_argb32_over_argb32_mask, 1, 1);

    run_test_series("10x10 argb32 source", test_argb32_source_argb32, 10, 10);
    run_test_series("10x10 argb32 over", test_argb32_over_argb32, 10, 10);
    run_test_series("10x10 argb32 over mask", test_argb32_over_argb32_mask, 10,
                    10);

    run_test_series("100x100 argb32 source", test_argb32_source_argb32, 100,
                    100);
    run_test_series("100x100 argb32 over", test_argb32_over_argb32, 100, 100);
    run_test_series("100x100 argb32 over mask", test_argb32_over_argb32_mask,
                    100, 100);
}

static void run_solid_tests(void)
{
    /* Solid color composition tests */
    run_test_series("1x1 solid source", test_solid_source_argb32, 1, 1);
    run_test_series("1x1 solid over", test_solid_over_argb32, 1, 1);
    run_test_series("1x1 solid over mask", test_solid_over_argb32_mask, 1, 1);

    run_test_series("10x10 solid source", test_solid_source_argb32, 10, 10);
    run_test_series("10x10 solid over", test_solid_over_argb32, 10, 10);
    run_test_series("10x10 solid over mask", test_solid_over_argb32_mask, 10,
                    10);

    run_test_series("100x100 solid source", test_solid_source_argb32, 100, 100);
    run_test_series("100x100 solid over", test_solid_over_argb32, 100, 100);
    run_test_series("100x100 solid over mask", test_solid_over_argb32_mask, 100,
                    100);
}

static void run_alpha_tests(void)
{
    /* Alpha transparency tests */
    run_test_series("100x100 solid over 25%", test_solid_over_argb32_25, 100,
                    100);
    run_test_series("100x100 solid over 50%", test_solid_over_argb32_50, 100,
                    100);
    run_test_series("100x100 solid over 75%", test_solid_over_argb32_75, 100,
                    100);
    run_test_series("100x100 solid over opaque", test_solid_over_argb32_opaque,
                    100, 100);
}

static void run_large_tests(void)
{
    /* Large area tests */
    run_test_series("500x500 argb32 source", test_argb32_source_argb32, 500,
                    500);
    run_test_series("500x500 argb32 over", test_argb32_over_argb32, 500, 500);
    run_test_series("500x500 solid over", test_solid_over_argb32, 500, 500);
}

int main(void)
{
    time_t now;
    char hostname[256];

    /* Print header similar to x11perf */
    time(&now);
    if (gethostname(hostname, sizeof(hostname)) != 0)
        strcpy(hostname, "localhost");

    printf("Mado performance tester on %s\n", hostname);
    printf("%s", ctime(&now));

    /* Measure sync time adjustment */
    measure_sync_time();
    printf("Sync time adjustment is %.4f msecs.\n\n", sync_time_adjustment);

    /* Create test pixmaps */
    src32 = twin_pixmap_from_file("assets/tux.png", TWIN_ARGB32);
    assert(src32);
    dst32 = twin_pixmap_create(TWIN_ARGB32, TEST_PIX_WIDTH, TEST_PIX_HEIGHT);
    assert(dst32);
    mask8 = twin_pixmap_create(TWIN_A8, TEST_PIX_WIDTH, TEST_PIX_HEIGHT);
    assert(mask8);

    /* Fill destination pixmap */
    twin_fill(dst32, 0x80112233, TWIN_SOURCE, 0, 0, TEST_PIX_WIDTH,
              TEST_PIX_HEIGHT);

    /* Create gradient mask for masking tests */
    twin_fill(mask8, 0x80808080, TWIN_SOURCE, 0, 0, TEST_PIX_WIDTH,
              TEST_PIX_HEIGHT);

    /* Pre-touch data */
    test_width = 1;
    test_height = 1;
    test_argb32_source_argb32();

    /* Run comprehensive test series */
    run_basic_tests();
    run_solid_tests();
    run_alpha_tests();
    run_large_tests();

    /* Cleanup */
    twin_pixmap_destroy(src32);
    twin_pixmap_destroy(dst32);
    twin_pixmap_destroy(mask8);

    return 0;
}
