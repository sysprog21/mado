/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include "apps.h"

#include "apps_animation.h"
#include "apps_calc.h"
#include "apps_clock.h"
#include "apps_image.h"
#include "apps_multi.h"
#include "apps_spline.h"

#if defined(CONFIG_DEMO_CELLPHONE)
#include "phone.h"
#endif

#define ASSET_PATH "assets/"

#if defined(CONFIG_DEMO_CELLPHONE)
static bool start_phone(twin_screen_t *screen, int x, int y, int w, int h)
{
    (void) x, (void) y, (void) w, (void) h;
    return phone_boot(screen);
}
#endif

#if defined(CONFIG_DEMO_CLOCK)
static bool start_clock(twin_screen_t *screen, int x, int y, int w, int h)
{
    return apps_clock_start(screen, "Clock", x, y, w, h);
}
#endif

#if defined(CONFIG_DEMO_CALCULATOR)
static bool start_calc(twin_screen_t *screen, int x, int y, int w, int h)
{
    return apps_calc_start(screen, "Calc", x, y, w, h);
}
#endif

#if defined(CONFIG_DEMO_SPLINE)
static bool start_spline(twin_screen_t *screen, int x, int y, int w, int h)
{
    return apps_spline_start(screen, "Spline", x, y, w, h);
}
#endif

#if defined(CONFIG_DEMO_ANIMATION)
/* Sized by the image */
static bool start_animation(twin_screen_t *screen, int x, int y, int w, int h)
{
    (void) w, (void) h;
    return apps_animation_start(screen, "Animation", ASSET_PATH "nyancat.gif",
                                x, y);
}
#endif

#if defined(CONFIG_DEMO_IMAGE)
/* Fixed size */
static bool start_image(twin_screen_t *screen, int x, int y, int w, int h)
{
    (void) w, (void) h;
    return apps_image_start(screen, "Vectors", x, y);
}
#endif

#if defined(CONFIG_DEMO_MULTI)
static bool start_multi(twin_screen_t *screen, int x, int y, int w, int h)
{
    return apps_multi_start(screen, "Demo", x, y, w, h);
}
#endif

static const apps_entry_t apps[] = {
#if defined(CONFIG_DEMO_CELLPHONE)
    /* The phone shell targets the mobile policy only; it lists the other
     * apps that run there on its home screen */
    {"Phone", TWIN_SYM_MOBILE, APPS_WM(TwinWmMobile), true, 0, 0, PHONE_W,
     PHONE_H, start_phone, phone_shutdown},
#endif
#if defined(CONFIG_DEMO_CLOCK)
    {"Clock", TWIN_SYM_CLOCK, APPS_WM_ALL, false, 10, 10, 200, 200, start_clock,
     NULL},
#endif
#if defined(CONFIG_DEMO_CALCULATOR)
    {"Calc", TWIN_SYM_CALCULATOR, APPS_WM_ALL, false, 100, 100, 200, 200,
     start_calc, NULL},
#endif
#if defined(CONFIG_DEMO_SPLINE)
    {"Spline", TWIN_SYM_SHUFFLE, APPS_WM_ALL, false, 20, 20, 400, 400,
     start_spline, NULL},
#endif
#if defined(CONFIG_DEMO_ANIMATION)
    {"Animation", TWIN_SYM_PLAY, APPS_WM_ALL, false, 20, 20, 0, 0,
     start_animation, NULL},
#endif
#if defined(CONFIG_DEMO_IMAGE)
    {"Vectors", TWIN_SYM_GLOBE, APPS_WM_ALL, false, 20, 20, 0, 0, start_image,
     NULL},
#endif
#if defined(CONFIG_DEMO_MULTI)
    {"Demo", TWIN_SYM_LIST, APPS_WM_ALL, false, 100, 100, 400, 400, start_multi,
     NULL},
#endif
    {0}, /* keeps the table valid when nothing is built */
};

#define N_APPS ((int) (sizeof(apps) / sizeof(apps[0])) - 1)

int apps_count(void)
{
    return N_APPS;
}

const apps_entry_t *apps_at(int idx)
{
    return idx >= 0 && idx < N_APPS ? &apps[idx] : NULL;
}
