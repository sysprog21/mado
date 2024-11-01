/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "twin.h"

#define TEST_PIX_WIDTH 1200
#define TEST_PIX_HEIGHT 800

static twin_pixmap_t *src32, *dst32;
static int twidth, theight, titers;

static void test_argb32_source_argb32(void)
{
    twin_operand_t srco = {.source_kind = TWIN_PIXMAP, .u.pixmap = src32};
    twin_composite(dst32, 0, 0, &srco, 0, 0, NULL, 0, 0, TWIN_SOURCE, twidth,
                   theight);
}

static void test_argb32_over_argb32(void)
{
    twin_operand_t srco = {.source_kind = TWIN_PIXMAP, .u.pixmap = src32};
    twin_composite(dst32, 0, 0, &srco, 0, 0, NULL, 0, 0, TWIN_OVER, twidth,
                   theight);
}

static void do_test(const char *name, void (*test)(void))
{
    struct timeval start, end;
    unsigned long long sus, eus;
    char spc[128];
    char *s;
    int i;

    printf("%s", name);

    gettimeofday(&start, NULL);
    for (i = 0; i < titers; i++)
        test();
    gettimeofday(&end, NULL);
    sus = (unsigned long long) start.tv_sec * 1000000ull + start.tv_usec;
    eus = (unsigned long long) end.tv_sec * 1000000ull + end.tv_usec;

    s = spc;
    for (i = strlen(name); i < 40; i++)
        *(s++) = ' ';
    *s = 0;
    printf("%s %f sec\n", spc, ((float) (eus - sus)) / 1000000.0);
}

#define DO_TEST(name) do_test(#name, test_##name)

static void do_tests(int width, int height, int iters)
{
    twidth = width;
    theight = height;
    titers = iters;

    DO_TEST(argb32_source_argb32);
    DO_TEST(argb32_over_argb32);
}

static void do_all_tests(const char *title,
                         int len_init,
                         int len_max,
                         int len_incre,
                         int iters)
{
    for (int len = len_init; len < len_max; len += len_incre) {
        printf("[ %s: %dx%dx%d ]\n", title, len, len, iters);
        do_tests(len, len, iters);
    }

    printf("\n");
}

int main(void)
{
    /* Create some test pixmaps */
    src32 = twin_pixmap_from_file("assets/tux.png", TWIN_ARGB32);
    assert(src32);
    dst32 = twin_pixmap_create(TWIN_ARGB32, TEST_PIX_WIDTH, TEST_PIX_HEIGHT);
    assert(dst32);

    /* fill pixmaps */
    twin_fill(dst32, 0x80112233, TWIN_SOURCE, 0, 0, TEST_PIX_WIDTH,
              TEST_PIX_HEIGHT);

    /* pre-touch data */
    test_argb32_source_argb32();

    do_all_tests("Pixmap", 7, 18, 2, 1000000);

    return 0;
}