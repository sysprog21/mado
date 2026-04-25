/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "twin_private.h"

#if defined(CONFIG_MEMORY_STATS)

/* Small inline tracker storage keeps the common case allocation-free,
 * but the table can grow with libc realloc so usage accounting remains
 * correct when many allocations are live at once.
 *
 * The tracking table itself always uses libc malloc/realloc/free, never
 * the raw allocator backend (twin_raw_*).  This avoids a circular
 * dependency when CONFIG_MEM_TLSF is enabled: the TLSF pool allocations
 * are tracked in the table, so the table storage must not come from the
 * same pool.
 */
#define MEMTBL_INLINE_CAPACITY 512

typedef struct {
    void *ptr;
    size_t size;
} memtbl_entry_t;

static memtbl_entry_t memtbl_inline[MEMTBL_INLINE_CAPACITY];
static memtbl_entry_t *memtbl = memtbl_inline;
static size_t memtbl_count;
static size_t memtbl_capacity = MEMTBL_INLINE_CAPACITY;

static twin_memstats_t stats;

static bool memtbl_grow(void)
{
    size_t new_capacity;
    if (memtbl_capacity > SIZE_MAX / 2 / sizeof(*memtbl))
        return false;

    new_capacity = memtbl_capacity * 2;
    memtbl_entry_t *new_table;

    if (memtbl == memtbl_inline) {
        new_table = malloc(new_capacity * sizeof(*new_table));
        if (!new_table)
            return false;
        memcpy(new_table, memtbl_inline, memtbl_count * sizeof(*new_table));
    } else {
        new_table = realloc(memtbl, new_capacity * sizeof(*new_table));
        if (!new_table)
            return false;
    }

    memtbl = new_table;
    memtbl_capacity = new_capacity;
    return true;
}

static ptrdiff_t memtbl_find(const void *ptr)
{
    for (size_t i = 0; i < memtbl_count; i++) {
        if (memtbl[i].ptr == ptr)
            return (ptrdiff_t) i;
    }
    return -1;
}

static bool memtbl_insert(void *ptr, size_t size)
{
    if (!ptr)
        return false;

    ptrdiff_t idx = memtbl_find(ptr);
    if (idx >= 0) {
        memtbl[idx].size = size;
        return true;
    }

    if (memtbl_count == memtbl_capacity && !memtbl_grow())
        return false;

    memtbl[memtbl_count].ptr = ptr;
    memtbl[memtbl_count].size = size;
    memtbl_count++;
    return true;
}

static size_t memtbl_remove(void *ptr)
{
    if (!ptr)
        return 0;

    ptrdiff_t idx = memtbl_find(ptr);
    if (idx < 0)
        return 0;

    size_t sz = memtbl[idx].size;
    memtbl_count--;
    if ((size_t) idx != memtbl_count)
        memtbl[idx] = memtbl[memtbl_count];
    memtbl[memtbl_count].ptr = NULL;
    memtbl[memtbl_count].size = 0;
    return sz;
}

void *_twin_malloc(size_t size, const char *file, int line)
{
    (void) file;
    (void) line;
    void *ptr = twin_raw_malloc(size);
    if (ptr) {
        if (!memtbl_insert(ptr, size)) {
            twin_raw_free(ptr);
            return NULL;
        }
        stats.current_bytes += size;
        if (stats.current_bytes > stats.peak_bytes)
            stats.peak_bytes = stats.current_bytes;
        stats.total_bytes += size;
        stats.total_allocs++;
    }
    return ptr;
}

void *_twin_calloc(size_t n, size_t size, const char *file, int line)
{
    (void) file;
    (void) line;
    /* Overflow check */
    if (n && size > SIZE_MAX / n)
        return NULL;
    void *ptr = twin_raw_calloc(n, size);
    if (ptr) {
        size_t total = n * size;
        if (!memtbl_insert(ptr, total)) {
            twin_raw_free(ptr);
            return NULL;
        }
        stats.current_bytes += total;
        if (stats.current_bytes > stats.peak_bytes)
            stats.peak_bytes = stats.current_bytes;
        stats.total_bytes += total;
        stats.total_allocs++;
    }
    return ptr;
}

void *_twin_realloc(void *old, size_t size, const char *file, int line)
{
    (void) file;
    (void) line;

    if (size == 0) {
        size_t old_size = memtbl_remove(old);
        stats.current_bytes -= old_size;
        if (old)
            stats.total_frees++;
        twin_raw_free(old);
        return NULL;
    }

    ptrdiff_t old_idx = old ? memtbl_find(old) : -1;
    if (old_idx < 0 && memtbl_count == memtbl_capacity && !memtbl_grow())
        return NULL;

    void *ptr = twin_raw_realloc(old, size);
    if (!ptr)
        return NULL;

    if (old_idx >= 0) {
        size_t old_size = memtbl[old_idx].size;
        memtbl[old_idx].ptr = ptr;
        memtbl[old_idx].size = size;
        stats.current_bytes -= old_size;
    } else {
        bool inserted = memtbl_insert(ptr, size);
        assert(inserted && "memtbl_insert should succeed after pre-growing");
        if (!inserted)
            return ptr;
    }

    stats.current_bytes += size;
    if (stats.current_bytes > stats.peak_bytes)
        stats.peak_bytes = stats.current_bytes;
    stats.total_bytes += size;
    stats.total_allocs++;
    if (old)
        stats.total_frees++;
    return ptr;
}

void _twin_free(void *ptr, const char *file, int line)
{
    (void) file;
    (void) line;
    if (!ptr)
        return;
    size_t sz = memtbl_remove(ptr);
    stats.current_bytes -= sz;
    stats.total_frees++;
    twin_raw_free(ptr);
}

void twin_memory_get_info(twin_memory_info_t *info)
{
    if (!info)
        return;
    info->current_bytes = stats.current_bytes;
    info->peak_bytes = stats.peak_bytes;
    info->total_allocs = stats.total_allocs;
    info->total_frees = stats.total_frees;
}

void twin_memory_reset_peak(void)
{
    stats.peak_bytes = stats.current_bytes;
}

#else /* !CONFIG_MEMORY_STATS */

void twin_memory_get_info(twin_memory_info_t *info)
{
    if (!info)
        return;
    memset(info, 0, sizeof(*info));
}

void twin_memory_reset_peak(void) {}

#endif /* CONFIG_MEMORY_STATS */
