/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Every app under every scene: the same app code must fill whatever client
 * area the policy gives it.  Each view's client area is flooded with a
 * sentinel colour, then repainted in full; any sentinel pixel left over is
 * a row or column the layout never reached.
 */

#include "phone-test.h"

#define SENTINEL 0xffff00ff

/* Pixel at pixmap coordinates, in the pixmap's own format */
static uint32_t raw_pixel(const twin_pixmap_t *p,
                          twin_coord_t x,
                          twin_coord_t y)
{
    const uint8_t *row = (const uint8_t *) p->p.v + (size_t) y * p->stride;
    if (p->format == TWIN_RGB16)
        return ((const uint16_t *) row)[x];
    return ((const uint32_t *) row)[x];
}

/* Count client pixels still holding the sentinel after a full repaint */
static int unpainted(ui_view_t *v)
{
    twin_window_t *w = v->window;
    twin_pixmap_t *p = w->pixmap;
    twin_rect_t c = w->client;

    /* twin_fill() works relative to the pixmap origin */
    twin_fill(p, SENTINEL, TWIN_SOURCE, c.left - p->origin_x,
              c.top - p->origin_y, c.right - p->origin_x,
              c.bottom - p->origin_y);
    uint32_t mark = raw_pixel(p, c.left, c.top);
    ui_view_invalidate_all(v);
    ph_wait(500);

    int left = 0;
    for (twin_coord_t y = c.top; y < c.bottom; y++)
        for (twin_coord_t x = c.left; x < c.right; x++)
            left += raw_pixel(p, x, y) == mark;
    return left;
}

/* Switch the scene under the open app @v and check it re-laid out */
static void check_scene(ui_view_t *v,
                        const char *name,
                        twin_wm_mode_t mode,
                        const char *scene,
                        twin_coord_t want_h)
{
    CHECK(phone_set_scene(mode), "%s under %s", name, scene);
    ph_wait(500);
    char what[64];
    snprintf(what, sizeof(what), "%s: %s view height", scene, name);
    if (want_h)
        CHECK_EQ(ui_view_height(v), want_h, what);
    int n = unpainted(v);
    CHECK(n == 0, "%s: %s left %d client pixels unpainted", scene, name, n);
}

/*
 * Open each app once and cycle the scenes underneath it.  Every switch
 * resizes the view, so this covers both laying out at creation (mobile)
 * and re-laying out in place for each other scene.
 */
void test_scenes(void)
{
    for (int i = 0; i < phone_app_count(); i++) {
        const char *name = phone_app_at(i)->name;
        ui_view_t *v = ph_open(name);
        if (!v)
            continue;
        CHECK_EQ(ui_view_height(v), PHONE_CONTENT_H, name);
        int n = unpainted(v);
        CHECK(n == 0, "mobile: %s left %d client pixels unpainted", name, n);
        check_scene(v, name, TwinWmNone, "none", PHONE_H);
        check_scene(v, name, TwinWmMobile, "mobile after none",
                    PHONE_CONTENT_H);
#if defined(CONFIG_WM_DESKTOP)
        check_scene(v, name, TwinWmDesktop, "desktop", 0);
        check_scene(v, name, TwinWmMobile, "mobile after desktop",
                    PHONE_CONTENT_H);
#endif
        ph_check(name);
        ph_home();
    }
}
