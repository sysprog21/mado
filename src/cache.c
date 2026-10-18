/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Coverage cache
 *
 * Anti-aliased rasterization dominates the cost of redrawing vector UI:
 * every glyph and rounded rectangle is scan-converted with 4x4 supersampling.
 * Most of that work repeats.  A list row, a keypad, or a home screen redraws
 * the same few dozen shapes, often at different positions.
 *
 * This cache keeps A8 coverage masks in a single LRU with a byte budget.
 * Glyph entries are keyed by (font, size, code point); path entries by a
 * hash of the device-space outline relative to the pixel grid, so the same
 * shape at another integer offset hits.  Because the rasterizer's sample grid
 * is pixel-aligned, a cached mask is bit-identical to a fresh one.
 *
 * Memory is bounded by the budget plus one hash table; entries larger than
 * CONFIG_CACHE_MAX_ENTRY are never admitted, so a full-screen fill cannot
 * flush the working set.
 */

#include "twin_private.h"

#define TWIN_CACHE_BUCKETS 128 /* power of two */

typedef struct {
    twin_cache_entry_t *buckets[TWIN_CACHE_BUCKETS];
    twin_cache_entry_t *head, *tail; /* most / least recently used */
    size_t bytes, peak_bytes;
    uint32_t entries, hits, misses, evictions;
} twin_cache_t;

/* The table starts zeroed in .bss; only the budget needs an initial value */
static twin_cache_t cache;
static size_t cache_budget = (size_t) CONFIG_CACHE_SIZE * 1024;

static inline unsigned _bucket(uint64_t key)
{
    return (unsigned) ((key ^ (key >> 29) ^ (key >> 47)) &
                       (TWIN_CACHE_BUCKETS - 1));
}

static void _lru_unlink(twin_cache_entry_t *e)
{
    if (e->prev)
        e->prev->next = e->next;
    else
        cache.head = e->next;
    if (e->next)
        e->next->prev = e->prev;
    else
        cache.tail = e->prev;
    e->prev = e->next = NULL;
}

static void _lru_push_front(twin_cache_entry_t *e)
{
    e->prev = NULL;
    e->next = cache.head;
    if (cache.head)
        cache.head->prev = e;
    cache.head = e;
    if (!cache.tail)
        cache.tail = e;
}

static void _cache_remove(twin_cache_entry_t *e)
{
    twin_cache_entry_t **pp = &cache.buckets[_bucket(e->key)];
    while (*pp && *pp != e)
        pp = &(*pp)->hnext;
    if (*pp)
        *pp = e->hnext;
    _lru_unlink(e);
    cache.bytes -= e->size;
    cache.entries--;
    twin_free(e);
}

static void _cache_shrink(size_t target)
{
    while (cache.tail && cache.bytes > target) {
        _cache_remove(cache.tail);
        cache.evictions++;
    }
}

twin_cache_entry_t *_twin_cache_entry_alloc(twin_coord_t width,
                                            twin_coord_t height)
{
    twin_coord_t stride = _twin_a8_stride(width);
    twin_cache_entry_t *e =
        twin_calloc(1, sizeof(twin_cache_entry_t) + (size_t) stride * height);
    if (!e)
        return NULL;
    e->width = width;
    e->height = height;
    e->stride = stride;
    return e;
}

bool _twin_cache_enabled(void)
{
    return cache_budget > 0;
}

twin_cache_entry_t *_twin_cache_lookup(uint64_t key)
{
    /* Disabled: the cache is not consulted, so this is neither a hit nor a
     * miss (path masks skip the lookup outright, see _twin_cache_enabled) */
    if (!cache_budget)
        return NULL;
    for (twin_cache_entry_t *e = cache.buckets[_bucket(key)]; e; e = e->hnext) {
        if (e->key == key) {
            if (e != cache.head) {
                _lru_unlink(e);
                _lru_push_front(e);
            }
            cache.hits++;
            return e;
        }
    }
    cache.misses++;
    return NULL;
}

twin_cache_entry_t *_twin_cache_insert(uint64_t key,
                                       twin_coord_t width,
                                       twin_coord_t height)
{
    if (!cache_budget || width < 0 || height < 0)
        return NULL;
    size_t data = (size_t) _twin_a8_stride(width) * (size_t) height;
    size_t size = sizeof(twin_cache_entry_t) + data;
    if (data > CONFIG_CACHE_MAX_ENTRY || size > cache_budget)
        return NULL;

    _cache_shrink(cache_budget - size);
    twin_cache_entry_t *e = _twin_cache_entry_alloc(width, height);
    if (!e)
        return NULL;
    e->key = key;
    e->size = (uint32_t) size;

    unsigned b = _bucket(key);
    e->hnext = cache.buckets[b];
    cache.buckets[b] = e;
    _lru_push_front(e);
    cache.bytes += size;
    cache.entries++;
    if (cache.bytes > cache.peak_bytes)
        cache.peak_bytes = cache.bytes;
    return e;
}

void twin_cache_set_budget(size_t bytes)
{
    cache_budget = bytes;
    _cache_shrink(bytes);
}

void twin_cache_flush(void)
{
    while (cache.tail)
        _cache_remove(cache.tail);
}

void twin_cache_get_stats(twin_cache_stats_t *stats)
{
    stats->hits = cache.hits;
    stats->misses = cache.misses;
    stats->evictions = cache.evictions;
    stats->entries = cache.entries;
    stats->bytes = cache.bytes;
    stats->peak_bytes = cache.peak_bytes;
    stats->budget = cache_budget;
}
