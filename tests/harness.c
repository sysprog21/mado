/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include "harness.h"

#include <stdlib.h>
#include <string.h>

int th_checks, th_failures;

static twin_time_t virtual_now = 1000;

static twin_time_t th_now(void)
{
    return virtual_now;
}

static void th_put_span(twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_argb32_t *pixels,
                        void *closure)
{
    th_t *t = closure;
    memcpy(t->fb + (size_t) top * t->width + left, pixels,
           sizeof(twin_argb32_t) * (size_t) (right - left));
}

th_t *th_create(twin_coord_t width, twin_coord_t height)
{
    th_t *t = calloc(1, sizeof(*t));
    if (!t)
        return NULL;
    _twin_set_time_source(th_now);
    t->width = width;
    t->height = height;
    t->fb = calloc((size_t) width * height, sizeof(twin_argb32_t));
    t->screen = twin_screen_create(width, height, NULL, th_put_span, t);
    if (!t->fb || !t->screen) {
        if (t->screen)
            twin_screen_destroy(t->screen);
        free(t->fb);
        free(t);
        return NULL;
    }
    th_frame(t);
    return t;
}

void th_destroy(th_t *t)
{
    if (!t)
        return;
    twin_screen_destroy(t->screen);
    /* Let deferred frees and orphaned timeouts drain */
    _twin_run_timeout();
    _twin_run_work();
    free(t->fb);
    free(t);
}

void th_resize(th_t *t, twin_coord_t width, twin_coord_t height)
{
    twin_argb32_t *fb = calloc((size_t) width * height, sizeof(*fb));
    if (!fb)
        return;
    free(t->fb);
    t->fb = fb;
    t->width = width;
    t->height = height;
    twin_screen_resize(t->screen, width, height);
}

void th_frame(th_t *t)
{
    _twin_run_timeout();
    _twin_run_work();
    if (twin_screen_damaged(t->screen))
        twin_screen_update(t->screen);
}

void th_advance(th_t *t, int ms)
{
    while (ms > 0) {
        int step = ms < 16 ? ms : 16;
        virtual_now += step;
        ms -= step;
        th_frame(t);
    }
    th_frame(t);
}

void th_settle(th_t *t, int max_ms)
{
    for (int spent = 0; spent < max_ms;) {
        th_frame(t);
        twin_time_t delay = _twin_timeout_delay();
        if (delay < 0 || delay > max_ms - spent)
            break;
        /* Never step past the budget */
        int step = max_ms - spent < 16 ? max_ms - spent : 16;
        virtual_now += step;
        spent += step;
    }
    th_frame(t);
}

static void th_pointer(th_t *t,
                       twin_event_kind_t kind,
                       twin_coord_t x,
                       twin_coord_t y)
{
    twin_event_t ev = {.kind = kind};
    ev.u.pointer.screen_x = x;
    ev.u.pointer.screen_y = y;
    ev.u.pointer.button = 1;
    twin_screen_dispatch(t->screen, &ev);
    th_frame(t);
}

void th_press(th_t *t, twin_coord_t x, twin_coord_t y)
{
    th_pointer(t, TwinEventButtonDown, x, y);
}

void th_move(th_t *t, twin_coord_t x, twin_coord_t y)
{
    th_pointer(t, TwinEventMotion, x, y);
}

void th_release(th_t *t, twin_coord_t x, twin_coord_t y)
{
    th_pointer(t, TwinEventButtonUp, x, y);
}

void th_tap(th_t *t, twin_coord_t x, twin_coord_t y)
{
    th_press(t, x, y);
    th_advance(t, 32);
    th_release(t, x, y);
}

void th_drag(th_t *t,
             twin_coord_t x0,
             twin_coord_t y0,
             twin_coord_t x1,
             twin_coord_t y1,
             int steps)
{
    th_press(t, x0, y0);
    for (int i = 1; i <= steps; i++) {
        virtual_now += 16;
        th_move(t, x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps);
    }
    th_release(t, x1, y1);
}

void th_key(th_t *t, twin_ucs4_t ucs4)
{
    twin_event_t ev = {.kind = TwinEventUcs4};
    ev.u.ucs4.ucs4 = ucs4;
    twin_screen_dispatch(t->screen, &ev);
    th_frame(t);
}

static uint32_t crc_table[256];

static void crc_init(void)
{
    if (crc_table[1])
        return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
        crc_table[i] = c;
    }
}

static uint32_t crc_update(uint32_t crc, const void *data, size_t len)
{
    const uint8_t *p = data;
    crc_init();
    while (len--)
        crc = crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return crc;
}

static uint32_t th_crc_bytes(const void *data, size_t len)
{
    return ~crc_update(~0u, data, len);
}

uint32_t th_crc_rect(const th_t *t, twin_rect_t r)
{
    uint32_t crc = ~0u;
    for (twin_coord_t y = r.top; y < r.bottom; y++)
        crc = crc_update(crc, t->fb + (size_t) y * t->width + r.left,
                         sizeof(twin_argb32_t) * (size_t) (r.right - r.left));
    return ~crc;
}

uint32_t th_crc(const th_t *t)
{
    return th_crc_rect(t, (twin_rect_t) {0, t->width, 0, t->height});
}

twin_argb32_t th_pixel(const th_t *t, twin_coord_t x, twin_coord_t y)
{
    return t->fb[(size_t) y * t->width + x];
}

bool th_painted(const th_t *t, const twin_pixmap_t *p)
{
    twin_coord_t right = p->x + p->width, bottom = p->y + p->height;
    if (right > t->width)
        right = t->width;
    if (bottom > t->height)
        bottom = t->height;
    twin_argb32_t first = th_pixel(t, p->x, p->y);
    for (twin_coord_t y = p->y; y < bottom; y++)
        for (twin_coord_t x = p->x; x < right; x++)
            if (th_pixel(t, x, y) != first)
                return true;
    return false;
}

twin_window_t *th_top_app(twin_screen_t *screen, int *count)
{
    twin_window_t *top = NULL;
    int n = 0;
    for (twin_window_t *w = screen->windows; w; w = w->wm_next)
        if (w->mapped && w->style == TwinWindowApplication) {
            top = w;
            n++;
        }
    if (count)
        *count = n;
    return top;
}

int th_damage_check(th_t *t)
{
    size_t n = (size_t) t->width * t->height;
    twin_argb32_t *saved = malloc(n * sizeof(*saved));
    if (!saved)
        return -1;

    /* Bring the incremental frame up to date first */
    if (twin_screen_damaged(t->screen))
        twin_screen_update(t->screen);
    memcpy(saved, t->fb, n * sizeof(*saved));

    twin_screen_stats_t stats = t->screen->stats;
    unsigned debug = _twin_debug;
    _twin_debug |= TWIN_DEBUG_NO_OCCLUSION;
    twin_screen_damage(t->screen, 0, 0, t->width, t->height);
    twin_screen_update(t->screen);
    _twin_debug = debug;
    t->screen->stats = stats;

    int diff = 0;
    for (size_t i = 0; i < n; i++) {
        if (saved[i] != t->fb[i]) {
            if (!diff)
                fprintf(stderr,
                        "  first stale pixel at (%zu,%zu): %08x != %08x\n",
                        i % t->width, i / t->width, saved[i], t->fb[i]);
            diff++;
        }
    }
    free(saved);
    return diff;
}

static bool th_save_ppm(const th_t *t, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    fprintf(f, "P6\n%d %d\n255\n", t->width, t->height);
    for (size_t i = 0; i < (size_t) t->width * t->height; i++) {
        twin_argb32_t p = t->fb[i];
        uint8_t rgb[3] = {(uint8_t) (p >> 16), (uint8_t) (p >> 8), (uint8_t) p};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return true;
}

size_t th_heap_raw(void)
{
    twin_memory_info_t info;
    twin_memory_get_info(&info);
    return info.current_bytes;
}

static bool th_heap_measured(void)
{
#if defined(CONFIG_MEMORY_STATS)
    return true;
#else
    static bool told;
    if (!told) {
        printf("memory checks not measured (MEMORY_STATS is off)\n");
        told = true;
    }
    return false;
#endif
}

size_t th_heap_net(const th_t *t)
{
    /* Without allocator statistics every reading is zero, so the leak and
     * budget checks compare 0 with 0: say so once instead of underflowing */
    if (!th_heap_measured())
        return 0;
    twin_cache_stats_t cs;
    twin_cache_get_stats(&cs);
    size_t retained = cs.bytes;
    /* Bounded per-screen reuse pools, not leaks */
    retained += (size_t) t->screen->path_cache_count * sizeof(twin_path_t);
    for (int i = 0; i < t->screen->path_cache_count; i++) {
        twin_path_t *p = t->screen->path_cache[i];
        if (p->points != p->inline_points)
            retained += (size_t) p->size_points * sizeof(twin_spoint_t);
        if (p->sublen != p->inline_sublen)
            retained += (size_t) p->size_sublen * sizeof(int);
    }
    retained += (size_t) t->screen->span_cache_width * sizeof(twin_argb32_t);
    return th_heap_raw() - retained;
}

/*
 * Golden checksums
 *
 * Pixel output depends on build options that change rendering (renderer,
 * shadow geometry, fonts, surface format, the JPEG decoder, the arithmetic
 * backend and its sine/cosine implementation), so the golden
 * file records a fingerprint of those options and comparisons are skipped
 * when it does not match.  Options that must not change pixels, such as the
 * coverage cache, are deliberately left out: those builds have to match.
 */
static uint32_t config_fingerprint(void)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
             "builtin=%d shadow=%d/%d/%d mont=%d icons=%d rgb16=%d jpeg=%d "
             "math=%d",
#if defined(CONFIG_RENDERER_BUILTIN)
             1,
#else
             0,
#endif
#if defined(CONFIG_SHADOW_BLUR) /* the desktop's shadow geometry */
             CONFIG_HORIZONTAL_OFFSET, CONFIG_VERTICAL_OFFSET,
             CONFIG_SHADOW_BLUR,
#elif defined(CONFIG_DROP_SHADOW) /* fixed geometry, as mobile slides */
             -1, -1, -1,
#else
             0, 0, 0,
#endif
#if defined(CONFIG_FONT_MONTSERRAT)
             1,
#else
             0,
#endif
#if defined(CONFIG_FONT_ICONS)
             1,
#else
             0,
#endif
#if defined(CONFIG_DEMO_CELLPHONE_RGB16)
             1,
#else
             0,
#endif
#if defined(CONFIG_LOADER_JPEG)
             1,
#else
             0,
#endif
#if defined(CONFIG_MATH_FLOAT) && CONFIG_MATH_FLOAT
             2
#elif defined(CONFIG_MATH_TRIG_TABLE) && CONFIG_MATH_TRIG_TABLE
             1
#else
             0
#endif
    );
    return th_crc_bytes(buf, strlen(buf));
}

#define MAX_GOLDEN 256

typedef struct {
    char name[64];
    uint32_t crc;
} golden_t;

static golden_t golden_new[MAX_GOLDEN];
static int n_golden_new;
static const char *golden_suite;
static int golden_skipped;

static const char *golden_dir(void)
{
    const char *d = getenv("MADO_GOLDEN_DIR");
    return d ? d : "tests/golden";
}

/* How a golden file's recorded configuration compares with this build */
typedef enum {
    GOLDEN_UNAVAILABLE, /* file missing, or no "# config" line */
    GOLDEN_OTHER_CONFIG,
    GOLDEN_SAME_CONFIG,
} golden_config_t;

static bool golden_lookup(const char *suite,
                          const char *name,
                          uint32_t *crc,
                          golden_config_t *config)
{
    char path[512], line[256];
    snprintf(path, sizeof(path), "%s/%s.txt", golden_dir(), suite);
    FILE *f = fopen(path, "r");
    *config = GOLDEN_UNAVAILABLE;
    if (!f)
        return false;
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        unsigned v;
        if (sscanf(line, "# config %x", &v) == 1) {
            *config = v == config_fingerprint() ? GOLDEN_SAME_CONFIG
                                                : GOLDEN_OTHER_CONFIG;
            continue;
        }
        if (sscanf(line, "%63s %x", key, &v) == 2 && !strcmp(key, name)) {
            *crc = v;
            found = true;
        }
    }
    fclose(f);
    return found;
}

void th_golden(const char *suite, const char *name, uint32_t crc, const th_t *t)
{
    const char *shots = getenv("MADO_SCREENSHOTS");
    char path[512];
    golden_suite = suite;

    if (shots && t) {
        snprintf(path, sizeof(path), "%s/%s-%s.ppm", shots, suite, name);
        th_save_ppm(t, path);
    }
    if (getenv("MADO_UPDATE_GOLDEN")) {
        if (n_golden_new < MAX_GOLDEN) {
            snprintf(golden_new[n_golden_new].name, sizeof(golden_new[0].name),
                     "%s", name);
            golden_new[n_golden_new++].crc = crc;
        }
        return;
    }

    uint32_t want = 0;
    golden_config_t config;
    bool found = golden_lookup(suite, name, &want, &config);
    /* Skip only a known configuration mismatch; a missing or malformed
     * file must not silently disable the comparison */
    if (config == GOLDEN_OTHER_CONFIG) {
        golden_skipped++;
        return;
    }
    if (config == GOLDEN_UNAVAILABLE) {
        CHECK(
            false,
            "%s/%s.txt missing or lacks a config line (run make check-update)",
            golden_dir(), suite);
        return;
    }
    CHECK(found, "no golden checksum for %s/%s (run make check-update)", suite,
          name);
    if (found && want != crc) {
        CHECK(want == crc, "%s/%s: crc %08x, golden %08x", suite, name, crc,
              want);
        if (t) {
            snprintf(path, sizeof(path), "%s-%s.fail.ppm", suite, name);
            th_save_ppm(t, path);
            fprintf(stderr, "  wrote %s\n", path);
        }
    }
}

int th_report(const char *suite)
{
    if (getenv("MADO_UPDATE_GOLDEN") && n_golden_new) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.txt", golden_dir(),
                 golden_suite ? golden_suite : suite);
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "# Golden frame checksums for the %s suite.\n", suite);
            fprintf(f, "# Regenerate with: make check-update\n");
            fprintf(f, "# config %08x\n", config_fingerprint());
            for (int i = 0; i < n_golden_new; i++)
                fprintf(f, "%s %08x\n", golden_new[i].name, golden_new[i].crc);
            fclose(f);
            printf("%s: wrote %d golden checksums to %s\n", suite, n_golden_new,
                   path);
        }
    }
    if (golden_skipped)
        printf(
            "%s: %d golden comparisons skipped (build options differ "
            "from the recorded configuration)\n",
            suite, golden_skipped);
    printf("%s: %d checks, %d failed\n", suite, th_checks, th_failures);
    return th_failures ? 1 : 0;
}
