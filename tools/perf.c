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
#include <time.h>
#include <unistd.h>

/* getrusage() is POSIX-specific for memory profiling
 * Not available on bare-metal or systems without full POSIX support
 */
#if defined(__unix__) || defined(__unix) || defined(unix) ||               \
    (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__) ||     \
    defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \
    defined(__HAIKU__)
#define HAVE_GETRUSAGE 1
#include <sys/resource.h>
#endif

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

/* Memory profiling iterations */
#define MEM_TEST_ITERATIONS 10000

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

/* Memory profiling mode */

/* Get memory usage statistics
 * Note: getrusage() requires POSIX support (Linux, macOS, etc.)
 * For bare-metal or embedded systems, return zero
 */
static void get_memory_usage(long *rss_kb, long *max_rss_kb)
{
#ifdef HAVE_GETRUSAGE
    /* POSIX systems with getrusage() support */
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
    *max_rss_kb = usage.ru_maxrss / 1024; /* macOS reports in bytes */
#else
    *max_rss_kb = usage.ru_maxrss; /* Linux reports in KB */
#endif
    *rss_kb = *max_rss_kb; /* Current RSS approximation */
#else
    /* Bare-metal or systems without getrusage() */
    *rss_kb = 0;
    *max_rss_kb = 0;
#endif
}

/* Print memory test statistics */
static void print_memory_stats(const char *test_name,
                               int iterations,
                               uint64_t elapsed_us,
                               long start_rss,
                               long end_rss,
                               long peak_rss)
{
    double ops_per_sec =
        (double) iterations / ((double) elapsed_us / 1000000.0);
    double us_per_op = (double) elapsed_us / iterations;
    double kops_per_sec = ops_per_sec / 1000.0;
    long delta_rss = end_rss - start_rss;

    printf("%-28s %6d %8.1f %9.1f %+8ld %7ld\n", test_name, iterations,
           us_per_op, kops_per_sec, delta_rss, peak_rss);
}

/* Memory test data structures */
struct mem_composite_test {
    twin_pixmap_t *dst, *src;
    int width, height;
    int iterations;
};

struct mem_polygon_test {
    twin_pixmap_t *dst;
    int iterations;
};

struct mem_pixmap_test {
    int width, height;
    int iterations;
};

struct mem_path_test {
    int iterations;
};

/* Memory test: Composite operations (xform buffer allocation) */
static void mem_test_composite(void *data)
{
    struct mem_composite_test *d = (struct mem_composite_test *) data;
    twin_operand_t srco = {.source_kind = TWIN_PIXMAP, .u.pixmap = d->src};

    for (int i = 0; i < d->iterations; i++) {
        twin_composite(d->dst, 0, 0, &srco, 0, 0, NULL, 0, 0, TWIN_OVER,
                       d->width, d->height);
    }
}

/* Memory test: Path operations (point reallocation) */
static void mem_test_path(void *data)
{
    struct mem_path_test *d = (struct mem_path_test *) data;

    for (int i = 0; i < d->iterations; i++) {
        twin_path_t *path = twin_path_create();

        /* Add many points to trigger reallocation */
        for (int j = 0; j < 100; j++) {
            twin_path_move(path, twin_int_to_fixed(j), twin_int_to_fixed(j));
            twin_path_draw(path, twin_int_to_fixed(j + 10),
                           twin_int_to_fixed(j + 10));
        }

        twin_path_destroy(path);
    }
}

/* Memory test: Polygon filling (edge buffer allocation) */
static void mem_test_polygon(void *data)
{
    struct mem_polygon_test *d = (struct mem_polygon_test *) data;

    for (int i = 0; i < d->iterations; i++) {
        twin_path_t *path = twin_path_create();

        /* Create a complex polygon */
        twin_path_move(path, twin_int_to_fixed(10), twin_int_to_fixed(10));
        twin_path_draw(path, twin_int_to_fixed(100), twin_int_to_fixed(10));
        twin_path_draw(path, twin_int_to_fixed(100), twin_int_to_fixed(100));
        twin_path_draw(path, twin_int_to_fixed(10), twin_int_to_fixed(100));
        twin_path_close(path);

        twin_paint_path(d->dst, 0xffff0000, path);

        twin_path_destroy(path);
    }
}

/* Memory test: Pixmap lifecycle */
static void mem_test_pixmap(void *data)
{
    struct mem_pixmap_test *d = (struct mem_pixmap_test *) data;

    for (int i = 0; i < d->iterations; i++) {
        twin_pixmap_t *pixmap =
            twin_pixmap_create(TWIN_ARGB32, d->width, d->height);
        twin_pixmap_destroy(pixmap);
    }
}

/* Run a memory profiling test */
static void run_memory_test(const char *test_name,
                            void (*test_func)(void *),
                            void *test_data,
                            int iterations)
{
    struct timeval start, end;
    long start_rss, end_rss, peak_rss;

    get_memory_usage(&start_rss, &peak_rss);

    gettimeofday(&start, NULL);
    test_func(test_data);
    gettimeofday(&end, NULL);

    get_memory_usage(&end_rss, &peak_rss);

    uint64_t elapsed_us = ((uint64_t) end.tv_sec * 1000000U + end.tv_usec) -
                          ((uint64_t) start.tv_sec * 1000000U + start.tv_usec);

    print_memory_stats(test_name, iterations, elapsed_us, start_rss, end_rss,
                       peak_rss);
}

/* Run complete memory profiling suite */
static void run_memory_profiling(void)
{
    printf("\n");
    printf(
        "Test                          Iters    us/op   kops/s  DeltaRS "
        "PeakRS\n");
    printf(
        "                                                          (KB)    "
        "(KB)\n");
    printf(
        "----------------------------------------------------------------------"
        "\n");

    /* Composite operations (xform buffer) */
    struct mem_composite_test comp_100 = {
        .dst = dst32,
        .src = src32,
        .width = 100,
        .height = 100,
        .iterations = MEM_TEST_ITERATIONS,
    };
    run_memory_test("100x100 comp (xform)", mem_test_composite, &comp_100,
                    MEM_TEST_ITERATIONS);

    struct mem_composite_test comp_500 = {
        .dst = dst32,
        .src = src32,
        .width = 500,
        .height = 500,
        .iterations = MEM_TEST_ITERATIONS / 10,
    };
    run_memory_test("500x500 comp (xform)", mem_test_composite, &comp_500,
                    MEM_TEST_ITERATIONS / 10);

    /* Path operations (point reallocation) */
    struct mem_path_test path_test = {.iterations = MEM_TEST_ITERATIONS / 10};
    run_memory_test("Path ops (realloc)", mem_test_path, &path_test,
                    MEM_TEST_ITERATIONS / 10);

    /* Polygon fill (edge buffer) */
    struct mem_polygon_test poly_test = {
        .dst = dst32,
        .iterations = MEM_TEST_ITERATIONS,
    };
    run_memory_test("Polygon fill (pool)", mem_test_polygon, &poly_test,
                    MEM_TEST_ITERATIONS);

    /* Pixmap lifecycle */
    struct mem_pixmap_test pixmap_100 = {
        .width = 100, .height = 100, .iterations = MEM_TEST_ITERATIONS};
    run_memory_test("100x100 pixmap life", mem_test_pixmap, &pixmap_100,
                    MEM_TEST_ITERATIONS);

    struct mem_pixmap_test pixmap_500 = {
        .width = 500, .height = 500, .iterations = MEM_TEST_ITERATIONS / 10};
    run_memory_test("500x500 pixmap life", mem_test_pixmap, &pixmap_500,
                    MEM_TEST_ITERATIONS / 10);

    printf("\nNotes:\n");
    printf("  DeltaRS: RSS change (+/- KB, negative = cleanup)\n");
    printf("  PeakRS: Maximum memory usage during test\n");
    printf("  Targets: xform (comp), pool (poly), realloc (path)\n");
}

int main(void)
{
    /* Print header */
    printf("Mado performance tester");
#ifdef HAVE_GETRUSAGE
    /* Full POSIX systems can provide hostname and timestamp */
    time_t now;
    char hostname[256];

    time(&now);
    if (gethostname(hostname, sizeof(hostname)) != 0)
        strcpy(hostname, "localhost");

    printf(" on %s\n", hostname);
    printf("%s", ctime(&now));
#else
    /* Bare-metal systems: minimal header */
    printf("\n");
#endif

    /* Measure sync time adjustment */
    measure_sync_time();
    printf("Sync time adjustment is %.4f msecs.\n\n", sync_time_adjustment);

    /* Create test pixmaps */
    src32 = twin_pixmap_from_file("assets/tux.png", TWIN_ARGB32);
    if (!src32) {
        /* Fallback: create a simple pixmap if file not found */
        src32 = twin_pixmap_create(TWIN_ARGB32, 256, 256);
        twin_fill(src32, 0xffff0000, TWIN_SOURCE, 0, 0, 256, 256);
    }
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

    /* Run comprehensive performance test series */
    run_basic_tests();
    run_solid_tests();
    run_alpha_tests();
    run_large_tests();

    /* Run memory profiling tests */
    printf("\n");
    printf("========================================\n");
    printf("  Memory Profiling\n");
    printf("========================================\n");
    run_memory_profiling();

    /* Cleanup */
    twin_pixmap_destroy(src32);
    twin_pixmap_destroy(dst32);
    twin_pixmap_destroy(mask8);

    return 0;
}
