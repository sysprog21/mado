/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Kit regressions that need no app on screen.
 */

#include <stdlib.h>
#include <string.h>

#include "phone-test.h"

#define FIT_W 200
#define FIT_H 24

static twin_pixmap_t *blank(void)
{
    twin_pixmap_t *px = twin_pixmap_create(TWIN_ARGB32, FIT_W, FIT_H);
    twin_fill(px, 0xffffffff, TWIN_SOURCE, 0, 0, FIT_W, FIT_H);
    return px;
}

static bool same_pixels(twin_pixmap_t *a, twin_pixmap_t *b)
{
    for (int y = 0; y < FIT_H; y++)
        if (memcmp(twin_pixmap_pointer(a, 0, y).v,
                   twin_pixmap_pointer(b, 0, y).v, FIT_W * 4))
            return false;
    return true;
}

/*
 * What ui_text_fit() promises: the longest prefix of whole characters
 * (trailing spaces dropped) that fits with "..." appended.
 */
static void expected_fit(int size, twin_coord_t room, const char *s, char *out)
{
    size_t best = 0;
    for (size_t n = 1; s[n - 1]; n++) {
        if (((unsigned char) s[n] & 0xc0) == 0x80)
            continue; /* not a character boundary */
        size_t m = n;
        while (m > 0 && s[m - 1] == ' ')
            m--;
        memcpy(out, s, m);
        strcpy(out + m, "...");
        if (ui_text_width(size, out) <= room)
            best = m;
    }
    memcpy(out, s, best);
    strcpy(out + best, "...");
}

/*
 * Truncation must not cut inside a UTF-8 sequence.  A cut there used to
 * end the measured string at the broken byte, so the stump "fit" and was
 * drawn without its ellipsis.
 */
static void check_fit(const char *s, twin_coord_t box_w, const char *what)
{
    const int size = 14;
    ui_rect_t r = ui_rect(0, 0, box_w, FIT_H);
    CHECK(ui_text_width(size, s) > box_w - 8, "%s: needs truncating", what);

    char want[128];
    expected_fit(size, box_w - 8, s, want);
    twin_pixmap_t *got = blank(), *ref = blank();
    ui_text_fit(got, size, r, UI_LEFT, 0xff000000, s);
    ui_text_in(ref, size, r, UI_LEFT, 0xff000000, want);
    CHECK(same_pixels(got, ref), "%s: drawn as \"%s\"", what, want);
    twin_pixmap_destroy(got);
    twin_pixmap_destroy(ref);
}

static void test_text_fit_utf8(void)
{
    /* Accented letters are two-byte sequences, so many cut points fall
     * mid-sequence.  (The bundled Montserrat subset has no glyph for them:
     * they measure zero, which does not matter to the cut.) */
    check_fit(
        "Jos\xc3\xa9 Mar\xc3\xad"
        "a Gonz\xc3\xa1lez Fern\xc3\xa1ndez",
        120, "accented name");
    for (twin_coord_t w = 40; w <= 120; w += 2)
        check_fit("Andr\xc3\xa9 \xc3\x89milie Ren\xc3\xa9\xc3\xa9 Zo\xc3\xab",
                  w, "every width");
}

static int color_distance(twin_argb32_t a, twin_argb32_t b)
{
    int d = 0;
    for (int shift = 0; shift < 24; shift += 8)
        d += abs((int) ((a >> shift) & 0xff) - (int) ((b >> shift) & 0xff));
    return d;
}

/* At home the Home tab is the current one, whichever apps are built: a
 * view without a launcher slot must not light the Dialer tab */
static void test_nav_at_home(void)
{
    CHECK_EQ(phone_depth(), 0, "at home");
    twin_argb32_t accent = phone_theme()->primary;
    twin_coord_t y = PHONE_H - PHONE_NAV_H + PHONE_NAV_H / 2;
    twin_argb32_t dialer = th_pixel(phone_th, 9, y);
    twin_argb32_t home = th_pixel(phone_th, PHONE_W / 3 + 9, y);
    CHECK(color_distance(home, accent) < color_distance(dialer, accent),
          "home tab lit at home (home %08x, dialer %08x)", home, dialer);
}

void test_regress(void)
{
    test_text_fit_utf8();
    test_nav_at_home();
}
