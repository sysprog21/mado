/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include "apps.h"

#if defined(CONFIG_DEMO_CELLPHONE)
#include "phone.h"
#endif

/* Every app below the phone runs under all three window managers */
static const apps_entry_t apps[] = {
#if defined(CONFIG_DEMO_CELLPHONE) && defined(CONFIG_WM_MOBILE)
    /* Mobile only: the phone shell lists the other apps on its home
     * screen */
    {"Phone", TWIN_SYM_MOBILE, 0, 0, PHONE_W, PHONE_H, NULL, phone_boot,
     phone_shutdown},
#endif
#if defined(CONFIG_DEMO_MULTI)
    {"Demo", TWIN_SYM_LIST, 100, 100, 400, 400, apps_multi_start, NULL, NULL},
#endif
#if defined(CONFIG_DEMO_CLOCK)
    {"Clock", TWIN_SYM_CLOCK, 10, 10, 200, 200, apps_clock_start, NULL, NULL},
#endif
#if defined(CONFIG_DEMO_CALCULATOR)
    {"Calculator", TWIN_SYM_CALCULATOR, 100, 100, 220, 300, apps_calc_start,
     NULL, NULL},
#endif
#if defined(CONFIG_DEMO_SPLINE)
    {"Spline", TWIN_SYM_SHUFFLE, 20, 20, 400, 400, apps_spline_start, NULL,
     NULL},
#endif
#if defined(CONFIG_DEMO_ANIMATION)
    {"Animation", TWIN_SYM_PLAY, 20, 20, 0, 0, apps_animation_start, NULL,
     NULL},
#endif
#if defined(CONFIG_DEMO_IMAGE)
    {"Vectors", TWIN_SYM_GLOBE, 20, 20, 0, 0, apps_image_start, NULL, NULL},
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
