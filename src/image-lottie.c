/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 *
 * Lottie animation support via Samsung rlottie.
 * Supports JSON (.json) and dotLottie (.lottie) formats.
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "twin.h"
#include "twin_private.h"

#include <rlottie_capi.h>
#include <zip.h>

/* Magic number to identify Lottie context */
#define MADO_LOTTIE_MAGIC 0x4C4F5454 /* "LOTT" */

#define DOTLOTTIE_MAX_PATH 512
#define DOTLOTTIE_MAX_JSON (16 * 1024 * 1024)

/*
 * Lottie image context
 */
typedef struct _mado_lottie_image {
    uint32_t magic;              /**< Magic number for identification */
    Lottie_Animation *animation; /**< rlottie handle */
    size_t width;
    size_t height;
    size_t total_frames;
    size_t current_frame;
    double framerate;
    uint32_t *buffer;            /**< ARGB8888 render buffer */
    bool playing;
    bool loop;
    char *temp_dir;              /**< For dotLottie extraction */
} mado_lottie_image_t;

/*
 * dotLottie helpers
 */

static bool is_zip_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

    uint8_t hdr[4];
    size_t n = fread(hdr, 1, 4, f);
    fclose(f);

    return (n == 4 && hdr[0] == 0x50 && hdr[1] == 0x4B &&
            hdr[2] == 0x03 && hdr[3] == 0x04);
}

static char *create_temp_dir(void)
{
    char tpl[] = "/tmp/mado_lottie_XXXXXX";
    char *d = mkdtemp(tpl);
    return d ? strdup(d) : NULL;
}

static void remove_temp_dir(const char *path)
{
    if (!path)
        return;
    char cmd[DOTLOTTIE_MAX_PATH + 16];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    int r = system(cmd);
    (void)r;
}

static bool zip_extract(zip_t *z, const char *name, const char *dest)
{
    zip_file_t *zf = zip_fopen(z, name, 0);
    if (!zf)
        return false;

    FILE *out = fopen(dest, "wb");
    if (!out) {
        zip_fclose(zf);
        return false;
    }

    char buf[8192];
    zip_int64_t n;
    while ((n = zip_fread(zf, buf, sizeof(buf))) > 0)
        fwrite(buf, 1, n, out);

    fclose(out);
    zip_fclose(zf);
    return true;
}

static char *zip_read(zip_t *z, const char *name)
{
    struct zip_stat st;
    zip_stat_init(&st);
    if (zip_stat(z, name, 0, &st) != 0 || st.size > DOTLOTTIE_MAX_JSON)
        return NULL;

    zip_file_t *zf = zip_fopen(z, name, 0);
    if (!zf)
        return NULL;

    char *data = malloc(st.size + 1);
    if (!data) {
        zip_fclose(zf);
        return NULL;
    }

    zip_fread(zf, data, st.size);
    data[st.size] = '\0';
    zip_fclose(zf);
    return data;
}

/* Simple JSON "id" extractor from animations array */
static char *get_animation_id(const char *manifest)
{
    const char *p = strstr(manifest, "\"animations\"");
    if (!p)
        return NULL;
    p = strchr(p, '[');
    if (!p)
        return NULL;
    p = strchr(p, '{');
    if (!p)
        return NULL;
    p = strstr(p, "\"id\"");
    if (!p)
        return NULL;
    p = strchr(p + 4, '"');
    if (!p)
        return NULL;
    p++;
    const char *e = strchr(p, '"');
    if (!e)
        return NULL;

    size_t len = e - p;
    char *id = malloc(len + 1);
    if (id) {
        memcpy(id, p, len);
        id[len] = '\0';
    }
    return id;
}

static bool mkdir_p(const char *path)
{
    char tmp[DOTLOTTIE_MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static char *extract_dotlottie(const char *path, char **out_temp)
{
    int err;
    zip_t *z = zip_open(path, ZIP_RDONLY, &err);
    if (!z)
        return NULL;

    char *temp = create_temp_dir();
    if (!temp) {
        zip_close(z);
        return NULL;
    }

    char *manifest = zip_read(z, "manifest.json");
    if (!manifest) {
        remove_temp_dir(temp);
        free(temp);
        zip_close(z);
        return NULL;
    }

    char *id = get_animation_id(manifest);
    free(manifest);
    if (!id) {
        remove_temp_dir(temp);
        free(temp);
        zip_close(z);
        return NULL;
    }

    /* Extract animation JSON */
    char zpath[DOTLOTTIE_MAX_PATH], lpath[DOTLOTTIE_MAX_PATH];
    snprintf(zpath, sizeof(zpath), "animations/%s.json", id);
    snprintf(lpath, sizeof(lpath), "%s/%s.json", temp, id);
    free(id);

    if (!zip_extract(z, zpath, lpath)) {
        remove_temp_dir(temp);
        free(temp);
        zip_close(z);
        return NULL;
    }

    /* Extract images */
    char imgdir[DOTLOTTIE_MAX_PATH];
    snprintf(imgdir, sizeof(imgdir), "%s/images", temp);
    mkdir_p(imgdir);

    zip_int64_t n = zip_get_num_entries(z, 0);
    for (zip_int64_t i = 0; i < n; i++) {
        const char *name = zip_get_name(z, i, 0);
        if (!name || strncmp(name, "images/", 7) != 0)
            continue;
        const char *fname = name + 7;
        if (*fname && fname[strlen(fname) - 1] != '/') {
            char dest[DOTLOTTIE_MAX_PATH];
            snprintf(dest, sizeof(dest), "%s/images/%s", temp, fname);
            zip_extract(z, name, dest);
        }
    }

    zip_close(z);
    *out_temp = temp;
    return strdup(lpath);
}

/*
 * Public API
 */

mado_lottie_image_t *mado_lottie_from_file(const char *path,
                                           size_t width,
                                           size_t height)
{
    if (!path)
        return NULL;

    Lottie_Animation *anim = NULL;
    char *temp_dir = NULL;

    if (is_zip_file(path)) {
        /* dotLottie */
        char *json_path = extract_dotlottie(path, &temp_dir);
        if (!json_path)
            return NULL;

        char res_path[DOTLOTTIE_MAX_PATH];
        snprintf(res_path, sizeof(res_path), "%s/images/", temp_dir);

        FILE *f = fopen(json_path, "rb");
        if (!f) {
            free(json_path);
            remove_temp_dir(temp_dir);
            free(temp_dir);
            return NULL;
        }

        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);

        char *json = malloc(sz + 1);
        if (!json) {
            fclose(f);
            free(json_path);
            remove_temp_dir(temp_dir);
            free(temp_dir);
            return NULL;
        }

        fread(json, 1, sz, f);
        json[sz] = '\0';
        fclose(f);

        anim = lottie_animation_from_data(json, json_path, res_path);
        free(json);
        free(json_path);

        if (!anim) {
            remove_temp_dir(temp_dir);
            free(temp_dir);
            return NULL;
        }
    } else {
        /* Plain JSON */
        anim = lottie_animation_from_file(path);
        if (!anim)
            return NULL;
    }

    mado_lottie_image_t *lot = calloc(1, sizeof(*lot));
    if (!lot) {
        lottie_animation_destroy(anim);
        if (temp_dir) {
            remove_temp_dir(temp_dir);
            free(temp_dir);
        }
        return NULL;
    }

    lot->magic = MADO_LOTTIE_MAGIC;
    lot->animation = anim;
    lot->temp_dir = temp_dir;

    size_t dw, dh;
    lottie_animation_get_size(anim, &dw, &dh);
    lot->width = width > 0 ? width : dw;
    lot->height = height > 0 ? height : dh;
    lot->total_frames = lottie_animation_get_totalframe(anim);
    lot->framerate = lottie_animation_get_framerate(anim);
    lot->current_frame = 0;
    lot->playing = true;
    lot->loop = true;

    if (lot->width == 0 || lot->height == 0 || lot->total_frames == 0) {
        lottie_animation_destroy(anim);
        if (temp_dir) {
            remove_temp_dir(temp_dir);
            free(temp_dir);
        }
        free(lot);
        return NULL;
    }

    lot->buffer = malloc(lot->width * lot->height * sizeof(uint32_t));
    if (!lot->buffer) {
        lottie_animation_destroy(anim);
        if (temp_dir) {
            remove_temp_dir(temp_dir);
            free(temp_dir);
        }
        free(lot);
        return NULL;
    }

    return lot;
}

void mado_lottie_destroy(mado_lottie_image_t *lot)
{
    if (!lot || lot->magic != MADO_LOTTIE_MAGIC)
        return;

    if (lot->animation)
        lottie_animation_destroy(lot->animation);
    if (lot->temp_dir) {
        remove_temp_dir(lot->temp_dir);
        free(lot->temp_dir);
    }
    free(lot->buffer);
    lot->magic = 0;
    free(lot);
}

void mado_lottie_render(mado_lottie_image_t *lot)
{
    if (!lot || lot->magic != MADO_LOTTIE_MAGIC || !lot->buffer)
        return;

    lottie_animation_render(lot->animation, lot->current_frame,
                            lot->buffer, lot->width, lot->height,
                            lot->width * sizeof(uint32_t));
}

void mado_lottie_render_to_pixmap(mado_lottie_image_t *lot, twin_pixmap_t *pix)
{
    if (!lot || lot->magic != MADO_LOTTIE_MAGIC || !pix ||
        pix->format != TWIN_ARGB32)
        return;

    mado_lottie_render(lot);

    /* BGRA -> ARGB */
    twin_pointer_t p = twin_pixmap_pointer(pix, 0, 0);
    size_t n = lot->width * lot->height;
    for (size_t i = 0; i < n; i++) {
        uint32_t px = lot->buffer[i];
        uint8_t b = px & 0xFF;
        uint8_t g = (px >> 8) & 0xFF;
        uint8_t r = (px >> 16) & 0xFF;
        uint8_t a = (px >> 24) & 0xFF;
        p.argb32[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

/*
 * Control API
 */

void mado_lottie_set_progress(mado_lottie_image_t *lot, float progress)
{
    if (!lot || lot->magic != MADO_LOTTIE_MAGIC)
        return;
    if (progress < 0.0f)
        progress = 0.0f;
    if (progress > 1.0f)
        progress = 1.0f;
    lot->current_frame = (size_t)(progress * (lot->total_frames - 1));
}

void mado_lottie_set_playback(mado_lottie_image_t *lot, bool playing)
{
    if (lot && lot->magic == MADO_LOTTIE_MAGIC)
        lot->playing = playing;
}

void mado_lottie_set_loop(mado_lottie_image_t *lot, bool loop)
{
    if (lot && lot->magic == MADO_LOTTIE_MAGIC)
        lot->loop = loop;
}

bool mado_lottie_advance_frame(mado_lottie_image_t *lot)
{
    if (!lot || lot->magic != MADO_LOTTIE_MAGIC || !lot->playing)
        return false;

    lot->current_frame++;
    if (lot->current_frame >= lot->total_frames) {
        if (lot->loop) {
            lot->current_frame = 0;
        } else {
            lot->current_frame = lot->total_frames - 1;
            lot->playing = false;
            return false;
        }
    }
    return true;
}

twin_time_t mado_lottie_get_frame_delay(const mado_lottie_image_t *lot)
{
    if (!lot || lot->magic != MADO_LOTTIE_MAGIC || lot->framerate <= 0)
        return 33;
    return (twin_time_t)(1000.0 / lot->framerate);
}

bool mado_lottie_is_playing(const mado_lottie_image_t *lot)
{
    return (lot && lot->magic == MADO_LOTTIE_MAGIC) ? lot->playing : false;
}

bool mado_lottie_is_looping(const mado_lottie_image_t *lot)
{
    return (lot && lot->magic == MADO_LOTTIE_MAGIC) ? lot->loop : false;
}

/*
 * twin_animation_t integration
 */

static twin_animation_t *create_twin_animation(mado_lottie_image_t *lot)
{
    if (!lot)
        return NULL;

    twin_animation_t *a = calloc(1, sizeof(*a));
    if (!a)
        return NULL;

    a->n_frames = lot->total_frames;
    a->loop = lot->loop;
    a->width = lot->width;
    a->height = lot->height;

    a->frames = malloc(sizeof(twin_pixmap_t *));
    if (!a->frames) {
        free(a);
        return NULL;
    }

    a->frames[0] = twin_pixmap_create(TWIN_ARGB32, lot->width, lot->height);
    if (!a->frames[0]) {
        free(a->frames);
        free(a);
        return NULL;
    }

    /* Store lottie pointer - use magic for safe retrieval */
    a->frames[0]->window = (twin_window_t *)lot;

    a->frame_delays = malloc(sizeof(twin_time_t));
    if (!a->frame_delays) {
        twin_pixmap_destroy(a->frames[0]);
        free(a->frames);
        free(a);
        return NULL;
    }
    a->frame_delays[0] = mado_lottie_get_frame_delay(lot);

    a->iter = malloc(sizeof(twin_animation_iter_t));
    if (!a->iter) {
        free(a->frame_delays);
        twin_pixmap_destroy(a->frames[0]);
        free(a->frames);
        free(a);
        return NULL;
    }

    a->iter->anim = a;
    a->iter->current_index = 0;
    a->iter->current_frame = a->frames[0];
    a->iter->current_delay = a->frame_delays[0];

    mado_lottie_render_to_pixmap(lot, a->frames[0]);

    return a;
}

bool twin_animation_is_lottie(const twin_animation_t *anim)
{
    if (!anim || !anim->frames || !anim->frames[0])
        return false;

    mado_lottie_image_t *lot = (mado_lottie_image_t *)anim->frames[0]->window;
    return (lot && lot->magic == MADO_LOTTIE_MAGIC);
}

mado_lottie_image_t *twin_animation_get_lottie(twin_animation_t *anim)
{
    if (!twin_animation_is_lottie(anim))
        return NULL;
    return (mado_lottie_image_t *)anim->frames[0]->window;
}

void twin_lottie_advance_frame(twin_animation_t *anim)
{
    mado_lottie_image_t *lot = twin_animation_get_lottie(anim);
    if (!lot)
        return;

    mado_lottie_advance_frame(lot);
    mado_lottie_render_to_pixmap(lot, anim->frames[0]);
    anim->iter->current_index = lot->current_frame;
}

void twin_lottie_animation_destroy(twin_animation_t *anim)
{
    if (!anim)
        return;

    if (anim->frames && anim->frames[0]) {
        mado_lottie_image_t *lot = twin_animation_get_lottie(anim);
        if (lot)
            mado_lottie_destroy(lot);
        anim->frames[0]->window = NULL;
        twin_pixmap_destroy(anim->frames[0]);
    }

    free(anim->frames);
    free(anim->frame_delays);
    free(anim->iter);
    free(anim);
}

/*
 * Image loader entry
 */

twin_pixmap_t *_twin_lottie_to_pixmap(const char *path, twin_format_t fmt)
{
    if (fmt != TWIN_ARGB32)
        return NULL;

    mado_lottie_image_t *lot = mado_lottie_from_file(path, 0, 0);
    if (!lot)
        return NULL;

    twin_animation_t *anim = create_twin_animation(lot);
    if (!anim) {
        mado_lottie_destroy(lot);
        return NULL;
    }

    twin_pixmap_t *pix = twin_pixmap_create(fmt, lot->width, lot->height);
    if (!pix) {
        twin_lottie_animation_destroy(anim);
        return NULL;
    }

    pix->animation = anim;

    memcpy(twin_pixmap_pointer(pix, 0, 0).argb32,
           twin_pixmap_pointer(anim->frames[0], 0, 0).argb32,
           lot->width * lot->height * sizeof(uint32_t));

    return pix;
}

twin_pixmap_t *twin_lottie_to_pixmap_scale(const char *path,
                                           twin_format_t fmt,
                                           twin_coord_t w,
                                           twin_coord_t h)
{
    if (fmt != TWIN_ARGB32)
        return NULL;

    mado_lottie_image_t *lot = mado_lottie_from_file(path, w, h);
    if (!lot)
        return NULL;

    twin_animation_t *anim = create_twin_animation(lot);
    if (!anim) {
        mado_lottie_destroy(lot);
        return NULL;
    }

    twin_pixmap_t *pix = twin_pixmap_create(fmt, w, h);
    if (!pix) {
        twin_lottie_animation_destroy(anim);
        return NULL;
    }

    pix->animation = anim;

    memcpy(twin_pixmap_pointer(pix, 0, 0).argb32,
           twin_pixmap_pointer(anim->frames[0], 0, 0).argb32,
           w * h * sizeof(uint32_t));

    return pix;
}