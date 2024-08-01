#include <string.h>
#include <strings.h>

#include "twin_private.h"

#if !defined(CONFIG_LOADER_PNG)
#define CONFIG_LOADER_PNG 0
#endif

#if !defined(CONFIG_LOADER_JPEG)
#define CONFIG_LOADER_JPEG 0
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
    )
/* clang-format on */

typedef enum {
    IMAGE_TYPE_unknown,
#define _(x) IMAGE_TYPE_##x,
    SUPPORTED_FORMATS
#undef _
} twin_image_format_t;

/* FIXME: Check the header of the given images to determine the supported image
 * formats instead of parsing the filename without checking its content.
 */
static twin_image_format_t image_type_from_name(const char *path)
{
    twin_image_format_t type = IMAGE_TYPE_unknown;
    const char *extname = strrchr(path, '.');
    if (!extname)
        return IMAGE_TYPE_unknown;
#if LOADER_HAS(PNG)
    else if (!strcasecmp(extname, ".png")) {
        type = IMAGE_TYPE_png;
    }
#endif
#if LOADER_HAS(JPEG)
    else if (!strcasecmp(extname, ".jpg") || !strcasecmp(extname, ".jpeg")) {
        type = IMAGE_TYPE_jpeg;
    }
#endif
    /* otherwise, unsupported format */

    return type;
}

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
    loader_func_t loader = image_loaders[image_type_from_name(path)];
    if (!loader)
        return NULL;
    return loader(path, fmt);
}
