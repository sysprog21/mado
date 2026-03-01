/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <string.h>
#include <strings.h>

#include "twin_private.h"

#if !defined(CONFIG_LOADER_PNG)
#define CONFIG_LOADER_PNG 0
#endif

#if !defined(CONFIG_LOADER_JPEG)
#define CONFIG_LOADER_JPEG 0
#endif

#if !defined(CONFIG_LOADER_GIF)
#define CONFIG_LOADER_GIF 0
#endif

#if !defined(CONFIG_LOADER_TVG)
#define CONFIG_LOADER_TVG 0
#endif

#if !defined(CONFIG_LOADER_LOTTIE)
#define CONFIG_LOADER_LOTTIE 0
#endif

/* Feature test macro */
#define LOADER_HAS(x) CONFIG_LOADER_##x

/* clang-format off */
#define SUPPORTED_FORMATS       \
    IIF(LOADER_HAS(PNG))(       \
        _(png)                  \
    )                           \
    IIF(LOADER_HAS(JPEG))(      \
        _(jpeg)                 \
    )                           \
    IIF(LOADER_HAS(GIF))(       \
        _(gif)                  \
    )                           \
    IIF(LOADER_HAS(TVG))(       \
        _(tvg)                  \
    )                           \
    IIF(LOADER_HAS(LOTTIE))(    \
        _(lottie)               \
    )
/* clang-format on */

typedef enum {
    IMAGE_TYPE_unknown,
#define _(x) IMAGE_TYPE_##x,
    SUPPORTED_FORMATS
#undef _
} twin_image_format_t;

/* Define the headers of supported image formats.
 * Each image format has a unique header, allowing the format to be determined
 * by inspecting the file header.
 * Supported formats:
 * - PNG:
 *   http://www.libpng.org/pub/png/spec/1.2/PNG-Rationale.html#R.PNG-file-signature
 * - JPEG:
 *   https://www.file-recovery.com/jpg-signature-format.htm
 * - GIF:
 *   https://www.file-recovery.com/gif-signature-format.htm
 * - TinyVG:
 *   https://tinyvg.tech/download/specification.pdf
 * - Lottie:
 *   JSON format - starts with '{' and contains "v" version key
 */
static const uint8_t header_png[8] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
};
static const uint8_t header_jpeg[3] = {0xFF, 0xD8, 0xFF};
static const uint8_t header_gif[4] = {0x47, 0x49, 0x46, 0x38};
static const uint8_t header_tvg[2] = {0x72, 0x56};
/* ZIP magic for dotLottie detection */
static const uint8_t header_zip[4] = {0x50, 0x4B, 0x03, 0x04};

#if LOADER_HAS(LOTTIE)
/**
 * Check if the file content looks like a Lottie JSON file.
 */
static bool is_lottie_json(const uint8_t *header, size_t len)
{
    /* Skip leading whitespace */
    size_t i = 0;
    while (i < len && (header[i] == ' ' || header[i] == '\t' ||
                       header[i] == '\n' || header[i] == '\r')) {
        i++;
    }

    /* Must start with '{' */
    if (i >= len || header[i] != '{')
        return false;

    /* Look for "v" key which indicates Lottie version */
    const char *str = (const char *)header;
    if (strstr(str, "\"v\"") || strstr(str, "\"v\":"))
        return true;

    return false;
}
#endif

static twin_image_format_t image_type_detect(const char *path)
{
    twin_image_format_t type = IMAGE_TYPE_unknown;
    FILE *file = fopen(path, "rb");
    if (!file) {
        log_error("Failed to open %s", path);
        return IMAGE_TYPE_unknown;
    }

    uint8_t header[8];
    size_t bytes_read = fread(header, 1, sizeof(header), file);
    fclose(file);

    if (bytes_read < 8) /* incomplete image file */
        return IMAGE_TYPE_unknown;
#if LOADER_HAS(PNG)
    else if (!memcmp(header, header_png, sizeof(header_png))) {
        type = IMAGE_TYPE_png;
    }
#endif
#if LOADER_HAS(JPEG)
    else if (!memcmp(header, header_jpeg, sizeof(header_jpeg))) {
        type = IMAGE_TYPE_jpeg;
    }
#endif
#if LOADER_HAS(GIF)
    else if (!memcmp(header, header_gif, sizeof(header_gif))) {
        type = IMAGE_TYPE_gif;
    }
#endif
#if LOADER_HAS(TVG)
    else if (!memcmp(header, header_tvg, sizeof(header_tvg))) {
        type = IMAGE_TYPE_tvg;
    }
#endif
#if LOADER_HAS(LOTTIE)
    else if (!memcmp(header, header_zip, sizeof(header_zip)) || is_lottie_json(header, bytes_read)) {
        return IMAGE_TYPE_lottie;
    }
#endif

    /* otherwise, unsupported format */
    return type;
}

/* Function prototypes for implementations */
#define _(x)                                                   \
    twin_pixmap_t *_twin_##x##_to_pixmap(const char *filepath, \
                                         twin_format_t fmt);
SUPPORTED_FORMATS
#undef _

typedef twin_pixmap_t *(*loader_func_t)(const char *, twin_format_t);

/* clang-format off */
static loader_func_t image_loaders[] = {
    [IMAGE_TYPE_unknown] = NULL,
#define _(x) [IMAGE_TYPE_##x] = _twin_##x##_to_pixmap,
    SUPPORTED_FORMATS
#undef _
};
/* clang-format on */

twin_pixmap_t *twin_pixmap_from_file(const char *path, twin_format_t fmt)
{
    loader_func_t loader = image_loaders[image_type_detect(path)];
    if (!loader)
        return NULL;
    return loader(path, fmt);
}
