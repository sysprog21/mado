/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <stdlib.h>
#include <string.h>

#include "twin_private.h"

#define TWIN_BW 0
#define TWIN_TITLE_HEIGHT 20

#define IS_ALIGNED(p, alignment) ((p % alignment) == 0)
#define ALIGN_UP(sz, alignment)                            \
    (((alignment) & ((alignment) - 1)) == 0                \
         ? (((sz) + (alignment) - 1) & ~((alignment) - 1)) \
         : ((((sz) + (alignment) - 1) / (alignment)) * (alignment)))

/* Cache configuration */
#ifdef CONFIG_PIXMAP_CACHE
#define TWIN_PIXMAP_CACHE_SIZE CONFIG_PIXMAP_CACHE_SIZE
#define TWIN_PIXMAP_CACHE_MAX_BYTES (CONFIG_PIXMAP_CACHE_MAX_KB * 1024)
#else
#define TWIN_PIXMAP_CACHE_SIZE 0
#define TWIN_PIXMAP_CACHE_MAX_BYTES 0
#endif

/* Cache key for pixmap lookup */
typedef struct _twin_cache_key {
    twin_format_t format;
    twin_coord_t width;
    twin_coord_t height;
} twin_cache_key_t;

/* Cache entry for a pixmap */
typedef struct _twin_cache_entry {
    twin_cache_key_t key;
    twin_pixmap_t *pixmap;
    size_t size;          /* Size in bytes */
    uint32_t ref_count;   /* Reference count */
    uint64_t last_access; /* For LRU eviction */
    struct _twin_cache_entry *next;
    struct _twin_cache_entry *prev;
} twin_cache_entry_t;

/* Pixmap cache structure */
typedef struct _twin_pixmap_cache {
    twin_cache_entry_t *entries;  /* Hash table entries */
    twin_cache_entry_t *lru_head; /* LRU list head (most recent) */
    twin_cache_entry_t *lru_tail; /* LRU list tail (least recent) */
    uint32_t count;               /* Current number of cached pixmaps */
    size_t total_size;            /* Total size of cached pixmaps */
    uint64_t access_counter;      /* Monotonic counter for LRU */
    uint64_t hits;                /* Cache hit count */
    uint64_t misses;              /* Cache miss count */
    uint64_t evictions;           /* Number of evictions */
} twin_pixmap_cache_t;

/* Global cache instance */
static twin_pixmap_cache_t *global_cache = NULL;

/* Cache control flag */
#ifdef CONFIG_PIXMAP_CACHE
static bool _twin_pixmap_cache_enabled = true;
#else
static bool _twin_pixmap_cache_enabled = false;
#endif

/* Forward declarations for cache functions */
static twin_pixmap_t *_twin_pixmap_cache_lookup(twin_format_t format,
                                                twin_coord_t width,
                                                twin_coord_t height);
static void _twin_pixmap_cache_store(twin_pixmap_t *pixmap);
static void _twin_pixmap_cache_release(twin_pixmap_t *pixmap);

twin_pixmap_t *twin_pixmap_create(twin_format_t format,
                                  twin_coord_t width,
                                  twin_coord_t height)
{
    /* Try to find in cache first */
    twin_pixmap_t *pixmap = _twin_pixmap_cache_lookup(format, width, height);
    if (pixmap)
        return pixmap;

    /* Not in cache, create new pixmap */
    twin_coord_t stride = twin_bytes_per_pixel(format) * width;
    /* Align stride to 4 bytes for proper uint32_t access in Pixman. */
    if (!IS_ALIGNED(stride, 4))
        stride = ALIGN_UP(stride, 4);

    twin_area_t space = (twin_area_t) stride * height;
    twin_area_t size = sizeof(twin_pixmap_t) + space;
    pixmap = malloc(size);
    if (!pixmap)
        return NULL;

    pixmap->screen = 0;
    pixmap->up = 0;
    pixmap->down = 0;
    pixmap->x = pixmap->y = 0;
    pixmap->format = format;
    pixmap->width = width;
    pixmap->height = height;
    twin_matrix_identity(&pixmap->transform);
    pixmap->clip.left = pixmap->clip.top = 0;
    pixmap->clip.right = pixmap->width;
    pixmap->clip.bottom = pixmap->height;
    pixmap->origin_x = pixmap->origin_y = 0;
    pixmap->stride = stride;
    pixmap->disable = 0;
    pixmap->animation = NULL;
#if defined(CONFIG_DROP_SHADOW)
    pixmap->shadow = false;
#endif
    pixmap->p.v = pixmap + 1;
    memset(pixmap->p.v, '\0', space);

    /* Store in cache for future use */
    _twin_pixmap_cache_store(pixmap);

    return pixmap;
}

twin_pixmap_t *twin_pixmap_create_const(twin_format_t format,
                                        twin_coord_t width,
                                        twin_coord_t height,
                                        twin_coord_t stride,
                                        twin_pointer_t pixels)
{
    twin_pixmap_t *pixmap = malloc(sizeof(twin_pixmap_t));
    if (!pixmap)
        return NULL;

    pixmap->screen = 0;
    pixmap->up = 0;
    pixmap->down = 0;
    pixmap->x = pixmap->y = 0;
    pixmap->format = format;
    pixmap->width = width;
    pixmap->height = height;
    twin_matrix_identity(&pixmap->transform);
    pixmap->clip.left = pixmap->clip.top = 0;
    pixmap->clip.right = pixmap->width;
    pixmap->clip.bottom = pixmap->height;
    pixmap->origin_x = pixmap->origin_y = 0;
    pixmap->stride = stride;
    pixmap->disable = 0;
    pixmap->p = pixels;
    return pixmap;
}

void twin_pixmap_destroy(twin_pixmap_t *pixmap)
{
    if (pixmap->screen)
        twin_pixmap_hide(pixmap);

    /* Release cache reference if any */
    _twin_pixmap_cache_release(pixmap);

    /* Always free the pixmap */
    free(pixmap);
}

void twin_pixmap_show(twin_pixmap_t *pixmap,
                      twin_screen_t *screen,
                      twin_pixmap_t *lower)
{
    if (pixmap->disable)
        twin_screen_disable_update(screen);

    if (lower == pixmap)
        lower = pixmap->down;

    if (pixmap->screen)
        twin_pixmap_hide(pixmap);

    pixmap->screen = screen;

    if (lower) {
        pixmap->down = lower;
        pixmap->up = lower->up;
        lower->up = pixmap;
        if (!pixmap->up)
            screen->top = pixmap;
    } else {
        pixmap->down = NULL;
        pixmap->up = screen->bottom;
        screen->bottom = pixmap;
        if (!pixmap->up)
            screen->top = pixmap;
    }

    twin_pixmap_damage(pixmap, 0, 0, pixmap->width, pixmap->height);
}

void twin_pixmap_hide(twin_pixmap_t *pixmap)
{
    twin_screen_t *screen = pixmap->screen;
    twin_pixmap_t **up, **down;

    if (!screen)
        return;

    twin_pixmap_damage(pixmap, 0, 0, pixmap->width, pixmap->height);

    if (pixmap->up)
        down = &pixmap->up->down;
    else
        down = &screen->top;

    if (pixmap->down)
        up = &pixmap->down->up;
    else
        up = &screen->bottom;

    *down = pixmap->down;
    *up = pixmap->up;

    pixmap->screen = 0;
    pixmap->up = 0;
    pixmap->down = 0;
    if (pixmap->disable)
        twin_screen_enable_update(screen);
}

twin_pointer_t twin_pixmap_pointer(twin_pixmap_t *pixmap,
                                   twin_coord_t x,
                                   twin_coord_t y)
{
    twin_pointer_t p;

    p.b = (pixmap->p.b + y * pixmap->stride +
           x * twin_bytes_per_pixel(pixmap->format));
    return p;
}

void twin_pixmap_enable_update(twin_pixmap_t *pixmap)
{
    if (--pixmap->disable == 0) {
        if (pixmap->screen)
            twin_screen_enable_update(pixmap->screen);
    }
}

void twin_pixmap_disable_update(twin_pixmap_t *pixmap)
{
    if (pixmap->disable++ == 0) {
        if (pixmap->screen)
            twin_screen_disable_update(pixmap->screen);
    }
}

void twin_pixmap_set_origin(twin_pixmap_t *pixmap,
                            twin_coord_t ox,
                            twin_coord_t oy)
{
    pixmap->origin_x = ox;
    pixmap->origin_y = oy;
}

void twin_pixmap_offset(twin_pixmap_t *pixmap,
                        twin_coord_t offx,
                        twin_coord_t offy)
{
    pixmap->origin_x += offx;
    pixmap->origin_y += offy;
}

void twin_pixmap_get_origin(twin_pixmap_t *pixmap,
                            twin_coord_t *ox,
                            twin_coord_t *oy)
{
    *ox = pixmap->origin_x;
    *oy = pixmap->origin_y;
}

void twin_pixmap_origin_to_clip(twin_pixmap_t *pixmap)
{
    pixmap->origin_x = pixmap->clip.left;
    pixmap->origin_y = pixmap->clip.top;
}

void twin_pixmap_clip(twin_pixmap_t *pixmap,
                      twin_coord_t left,
                      twin_coord_t top,
                      twin_coord_t right,
                      twin_coord_t bottom)
{
    left += pixmap->origin_x;
    right += pixmap->origin_x;
    top += pixmap->origin_y;
    bottom += pixmap->origin_y;

    if (left > pixmap->clip.left)
        pixmap->clip.left = left;
    if (top > pixmap->clip.top)
        pixmap->clip.top = top;
    if (right < pixmap->clip.right)
        pixmap->clip.right = right;
    if (bottom < pixmap->clip.bottom)
        pixmap->clip.bottom = bottom;

    if (pixmap->clip.left >= pixmap->clip.right)
        pixmap->clip.right = pixmap->clip.left = 0;
    if (pixmap->clip.top >= pixmap->clip.bottom)
        pixmap->clip.bottom = pixmap->clip.top = 0;

    if (pixmap->clip.left < 0)
        pixmap->clip.left = 0;
    if (pixmap->clip.top < 0)
        pixmap->clip.top = 0;
    if (pixmap->clip.right > pixmap->width)
        pixmap->clip.right = pixmap->width;
    if (pixmap->clip.bottom > pixmap->height)
        pixmap->clip.bottom = pixmap->height;
}

void twin_pixmap_set_clip(twin_pixmap_t *pixmap, twin_rect_t clip)
{
    twin_pixmap_clip(pixmap, clip.left, clip.top, clip.right, clip.bottom);
}


twin_rect_t twin_pixmap_get_clip(twin_pixmap_t *pixmap)
{
    twin_rect_t clip = pixmap->clip;

    clip.left -= pixmap->origin_x;
    clip.right -= pixmap->origin_x;
    clip.top -= pixmap->origin_y;
    clip.bottom -= pixmap->origin_y;

    return clip;
}

twin_rect_t twin_pixmap_save_clip(twin_pixmap_t *pixmap)
{
    return pixmap->clip;
}

void twin_pixmap_restore_clip(twin_pixmap_t *pixmap, twin_rect_t rect)
{
    pixmap->clip = rect;
}

void twin_pixmap_reset_clip(twin_pixmap_t *pixmap)
{
    pixmap->clip.left = 0;
    pixmap->clip.top = 0;
    pixmap->clip.right = pixmap->width;
    pixmap->clip.bottom = pixmap->height;
}

void twin_pixmap_damage(twin_pixmap_t *pixmap,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom)
{
    if (pixmap->screen)
        twin_screen_damage(pixmap->screen, left + pixmap->x, top + pixmap->y,
                           right + pixmap->x, bottom + pixmap->y);
}

static twin_argb32_t _twin_pixmap_fetch(twin_pixmap_t *pixmap,
                                        twin_coord_t x,
                                        twin_coord_t y)
{
    twin_pointer_t p =
        twin_pixmap_pointer(pixmap, x - pixmap->x, y - pixmap->y);
    /* FIXME: for transform */

    if (pixmap->x <= x && x < pixmap->x + pixmap->width && pixmap->y <= y &&
        y < pixmap->y + pixmap->height) {
        switch (pixmap->format) {
        case TWIN_A8:
            return *p.a8 << 24;
        case TWIN_RGB16:
            return twin_rgb16_to_argb32(*p.rgb16);
        case TWIN_ARGB32:
            return *p.argb32;
        }
    }
    return 0;
}

bool twin_pixmap_transparent(twin_pixmap_t *pixmap,
                             twin_coord_t x,
                             twin_coord_t y)
{
    return (_twin_pixmap_fetch(pixmap, x, y) >> 24) == 0;
}

bool twin_pixmap_is_iconified(twin_pixmap_t *pixmap, twin_coord_t y)
{
    /*
     * Check whether the specified area within the pixmap corresponds to an
     * iconified window.
     */
    if (pixmap->window &&
        (pixmap->window->iconify &&
         y >= pixmap->y + TWIN_BW + TWIN_TITLE_HEIGHT + TWIN_BW))
        return true;
    return false;
}

void twin_pixmap_move(twin_pixmap_t *pixmap, twin_coord_t x, twin_coord_t y)
{
    twin_pixmap_damage(pixmap, 0, 0, pixmap->width, pixmap->height);
    pixmap->x = x;
    pixmap->y = y;
    twin_pixmap_damage(pixmap, 0, 0, pixmap->width, pixmap->height);
}

bool twin_pixmap_dispatch(twin_pixmap_t *pixmap, twin_event_t *event)
{
    if (pixmap->window)
        return twin_window_dispatch(pixmap->window, event);
    return false;
}

/*
 * Pixmap cache implementation
 */

#ifdef CONFIG_PIXMAP_CACHE

/* Calculate hash for cache key */
static uint32_t _twin_cache_hash(const twin_cache_key_t *key)
{
    uint32_t hash = 0;
    hash ^= key->format * 2654435761U;
    hash ^= key->width * 2246822519U;
    hash ^= key->height * 3266489917U;
    return hash % TWIN_PIXMAP_CACHE_SIZE;
}

/* Compare cache keys */
static bool _twin_cache_key_equal(const twin_cache_key_t *a,
                                  const twin_cache_key_t *b)
{
    return a->format == b->format && a->width == b->width &&
           a->height == b->height;
}

/* Calculate pixmap size in bytes */
static size_t _twin_pixmap_size(twin_format_t format,
                                twin_coord_t width,
                                twin_coord_t height)
{
    twin_coord_t stride = twin_bytes_per_pixel(format) * width;
    if (stride % 4 != 0)
        stride = ((stride + 3) / 4) * 4;
    return sizeof(twin_pixmap_t) + (size_t) stride * height;
}

/* Move entry to front of LRU list */
static void _twin_cache_lru_touch(twin_pixmap_cache_t *cache,
                                  twin_cache_entry_t *entry)
{
    /* Update access time */
    entry->last_access = ++cache->access_counter;

    /* Already at head */
    if (entry == cache->lru_head)
        return;

    /* Remove from current position */
    if (entry->prev)
        entry->prev->next = entry->next;
    if (entry->next)
        entry->next->prev = entry->prev;
    if (entry == cache->lru_tail)
        cache->lru_tail = entry->prev;

    /* Insert at head */
    entry->prev = NULL;
    entry->next = cache->lru_head;
    if (cache->lru_head)
        cache->lru_head->prev = entry;
    cache->lru_head = entry;
    if (!cache->lru_tail)
        cache->lru_tail = entry;
}

/* Remove entry from LRU list */
static void _twin_cache_lru_remove(twin_pixmap_cache_t *cache,
                                   twin_cache_entry_t *entry)
{
    if (entry->prev)
        entry->prev->next = entry->next;
    else
        cache->lru_head = entry->next;

    if (entry->next)
        entry->next->prev = entry->prev;
    else
        cache->lru_tail = entry->prev;

    entry->next = entry->prev = NULL;
}

/* Find the least recently used entry that can be evicted */
static twin_cache_entry_t *_twin_cache_find_evictable(
    twin_pixmap_cache_t *cache)
{
    twin_cache_entry_t *oldest = NULL;
    uint64_t oldest_time = UINT64_MAX;

    /* Find LRU entry with ref_count == 1 (only cached reference) */
    for (uint32_t i = 0; i < TWIN_PIXMAP_CACHE_SIZE; i++) {
        twin_cache_entry_t *entry = &cache->entries[i];
        if (entry->pixmap && entry->ref_count == 1 &&
            entry->last_access < oldest_time) {
            oldest = entry;
            oldest_time = entry->last_access;
        }
    }

    return oldest;
}

/* Initialize the pixmap cache (internal, called lazily) */
static void _twin_pixmap_cache_init(void)
{
    if (global_cache)
        return;

    global_cache = calloc(1, sizeof(twin_pixmap_cache_t));
    if (!global_cache)
        return;

    global_cache->entries =
        calloc(TWIN_PIXMAP_CACHE_SIZE, sizeof(twin_cache_entry_t));
    if (!global_cache->entries) {
        free(global_cache);
        global_cache = NULL;
        return;
    }
}

/* Look up a pixmap in the cache */
static twin_pixmap_t *_twin_pixmap_cache_lookup(twin_format_t format,
                                                twin_coord_t width,
                                                twin_coord_t height)
{
    if (!_twin_pixmap_cache_enabled)
        return NULL;

    /* Lazy initialization */
    if (!global_cache)
        _twin_pixmap_cache_init();

    if (!global_cache)
        return NULL;

    twin_cache_key_t key = {.format = format, .width = width, .height = height};

    uint32_t index = _twin_cache_hash(&key);
    twin_cache_entry_t *entry = &global_cache->entries[index];

    if (entry->pixmap && _twin_cache_key_equal(&entry->key, &key)) {
        /* Cache hit */
        global_cache->hits++;
        entry->ref_count++;
        _twin_cache_lru_touch(global_cache, entry);
        return entry->pixmap;
    }

    /* Cache miss */
    global_cache->misses++;
    return NULL;
}

/* Store a pixmap in the cache */
static void _twin_pixmap_cache_store(twin_pixmap_t *pixmap)
{
    if (!pixmap || !_twin_pixmap_cache_enabled)
        return;

    /* Lazy initialization */
    if (!global_cache)
        _twin_pixmap_cache_init();

    if (!global_cache)
        return;

    twin_cache_key_t key = {
        .format = pixmap->format,
        .width = pixmap->width,
        .height = pixmap->height,
    };

    size_t size =
        _twin_pixmap_size(pixmap->format, pixmap->width, pixmap->height);

    /* Don't cache if too large */
    if (size > TWIN_PIXMAP_CACHE_MAX_BYTES / 2)
        return;

    uint32_t index = _twin_cache_hash(&key);
    twin_cache_entry_t *entry = &global_cache->entries[index];

    /* Check if slot is occupied */
    if (entry->pixmap) {
        /* If same key, increment reference count */
        if (_twin_cache_key_equal(&entry->key, &key)) {
            entry->ref_count++;
            _twin_cache_lru_touch(global_cache, entry);
            return;
        }

        /* Different key - try to evict if possible */
        if (entry->ref_count > 1) {
            /* Entry is in use, try to find another slot to evict */
            twin_cache_entry_t *evict =
                _twin_cache_find_evictable(global_cache);
            if (!evict)
                return; /* No evictable entries */
            entry = evict;
        }

        /* Evict the current entry */
        _twin_cache_lru_remove(global_cache, entry);
        global_cache->count--;
        global_cache->total_size -= entry->size;
        global_cache->evictions++;

        /* Clear the entry */
        *entry = (twin_cache_entry_t){0};
    }

    /* Check if we need to evict for size constraints */
    while (global_cache->total_size + size > TWIN_PIXMAP_CACHE_MAX_BYTES &&
           global_cache->count > 0) {
        twin_cache_entry_t *evict = _twin_cache_find_evictable(global_cache);
        if (!evict)
            break; /* No more evictable entries */

        _twin_cache_lru_remove(global_cache, evict);
        global_cache->count--;
        global_cache->total_size -= evict->size;
        global_cache->evictions++;
        *evict = (twin_cache_entry_t){0};
    }

    /* Store new entry */
    entry->key = key;
    entry->pixmap = pixmap;
    entry->size = size;
    entry->ref_count = 1;
    entry->last_access = ++global_cache->access_counter;

    /* Add to LRU list */
    entry->next = global_cache->lru_head;
    entry->prev = NULL;
    if (global_cache->lru_head)
        global_cache->lru_head->prev = entry;
    global_cache->lru_head = entry;
    if (!global_cache->lru_tail)
        global_cache->lru_tail = entry;

    /* Update stats */
    global_cache->count++;
    global_cache->total_size += size;
}

/* Release a reference to a cached pixmap */
static void _twin_pixmap_cache_release(twin_pixmap_t *pixmap)
{
    if (!global_cache || !pixmap)
        return;

    /* Find and remove the entry */
    for (uint32_t i = 0; i < TWIN_PIXMAP_CACHE_SIZE; i++) {
        twin_cache_entry_t *entry = &global_cache->entries[i];
        if (entry->pixmap == pixmap) {
            entry->ref_count--;

            /* Remove from cache when destroyed */
            _twin_cache_lru_remove(global_cache, entry);
            global_cache->count--;
            global_cache->total_size -= entry->size;

            /* Clear the entry */
            *entry = (twin_cache_entry_t){0};
            return;
        }
    }
}


#else /* !CONFIG_PIXMAP_CACHE */

/* Stub implementations when cache is disabled */
static twin_pixmap_t *_twin_pixmap_cache_lookup(twin_format_t format,
                                                twin_coord_t width,
                                                twin_coord_t height)
{
    return NULL;
}

static void _twin_pixmap_cache_store(twin_pixmap_t *pixmap) {}

static void _twin_pixmap_cache_release(twin_pixmap_t *pixmap) {}

#endif /* CONFIG_PIXMAP_CACHE */
