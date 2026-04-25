/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 *
 * Minimal TLSF (Two-Level Segregated Fit) allocator for embedded targets.
 * Based on tlsf-bsd (BSD-3-Clause), stripped to static-pool essentials:
 * pool_init, malloc, realloc, free.  No dynamic growth, no aligned alloc,
 * no debug/check/stats walkers, no ASan/poison hooks.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "twin_private.h"

#if defined(CONFIG_MEM_TLSF)

/* -- TLSF configuration ------------------------------------------------- */

#define ALIGN_SIZE ((size_t) 1 << ALIGN_SHIFT)
#if __SIZE_WIDTH__ == 64
#define ALIGN_SHIFT 3
#else
#define ALIGN_SHIFT 2
#endif

#define SL_SHIFT 5
#define SL_COUNT (1U << SL_SHIFT)

/* Cap FL to keep control structure small for embedded pools.
 * FL_MAX = 24 supports pools up to 8 MB, which exceeds any sane
 * CONFIG_MEM_TLSF_POOL_SIZE for an embedded UI toolkit.
 */
#define FL_MAX 24

#define FL_SHIFT (SL_SHIFT + ALIGN_SHIFT)
#define FL_COUNT (FL_MAX - FL_SHIFT + 1)

#define BLOCK_BIT_FREE ((size_t) 1)
#define BLOCK_BIT_PREV_FREE ((size_t) 2)
#define BLOCK_BITS (BLOCK_BIT_FREE | BLOCK_BIT_PREV_FREE)

#define BLOCK_OVERHEAD (sizeof(size_t))
#define BLOCK_SIZE_MIN (sizeof(tlsf_block_t) - sizeof(tlsf_block_t *))
#define BLOCK_SIZE_MAX ((size_t) 1 << (FL_MAX - 1))
#define BLOCK_SIZE_SMALL ((size_t) 1 << FL_SHIFT)

#define TLSF_MAX_SIZE (((size_t) 1 << (FL_MAX - 1)) - sizeof(size_t))

#define UNLIKELY(x) __builtin_expect(!!(x), false)

/* Force inlining for TLSF internals -- these are hot-path helpers that
 * must inline even at -O0 to keep allocation cost bounded.
 */
#define TLSF_INLINE static inline __attribute__((always_inline))

/* -- TLSF data structures ------------------------------------------------ */

typedef struct tlsf_block {
    struct tlsf_block *prev;
    size_t header;
    struct tlsf_block *next_free, *prev_free;
} tlsf_block_t;

typedef struct {
    uint32_t fl, sl[FL_COUNT];
    void *arena;
    size_t size;
    struct tlsf_block *block[FL_COUNT][SL_COUNT];
    struct tlsf_block block_null;
} tlsf_t;

/* -- Helpers -------------------------------------------------------------- */

/* Use portable CLZ wrappers from twin_private.h (twin_clz / twin_clzll)
 * instead of raw __builtin_clz* so this compiles on MSVC and generic
 * targets without GCC/Clang intrinsics.
 */
/* Precondition: x != 0.  CLZ of zero is undefined on all platforms. */
TLSF_INLINE uint32_t bitmap_ffs(uint32_t x)
{
    /* ctz(x) = 31 - clz(x & -x) for nonzero x */
    return (uint32_t) (31 - twin_clz(x & (~x + 1)));
}

/* Precondition: x != 0. */
TLSF_INLINE uint32_t log2floor(size_t x)
{
#if __SIZE_WIDTH__ == 64
    return (uint32_t) (63 - twin_clzll(x));
#else
    return (uint32_t) (31 - twin_clz((uint32_t) x));
#endif
}

TLSF_INLINE size_t block_size(const tlsf_block_t *block)
{
    return block->header & ~BLOCK_BITS;
}

TLSF_INLINE void block_set_size(tlsf_block_t *block, size_t size)
{
    block->header = size | (block->header & BLOCK_BITS);
}

TLSF_INLINE bool block_is_free(const tlsf_block_t *block)
{
    return !!(block->header & BLOCK_BIT_FREE);
}

TLSF_INLINE bool block_is_prev_free(const tlsf_block_t *block)
{
    return !!(block->header & BLOCK_BIT_PREV_FREE);
}

TLSF_INLINE void block_set_prev_free(tlsf_block_t *block, bool free)
{
    block->header = free ? block->header | BLOCK_BIT_PREV_FREE
                         : block->header & ~BLOCK_BIT_PREV_FREE;
}

TLSF_INLINE size_t align_up(size_t x, size_t align)
{
    return (((x - 1) | (align - 1)) + 1);
}

TLSF_INLINE char *align_ptr(char *p, size_t align)
{
    uintptr_t addr = (uintptr_t) p;
    uintptr_t aligned = align_up(addr, align);
    return p + (aligned - addr);
}

TLSF_INLINE char *block_payload(tlsf_block_t *block)
{
    return (char *) block + offsetof(tlsf_block_t, header) + BLOCK_OVERHEAD;
}

TLSF_INLINE tlsf_block_t *to_block(void *ptr)
{
    return (tlsf_block_t *) ptr;
}

TLSF_INLINE tlsf_block_t *block_from_payload(void *ptr)
{
    return to_block((char *) ptr - offsetof(tlsf_block_t, header) -
                    BLOCK_OVERHEAD);
}

TLSF_INLINE tlsf_block_t *block_next(tlsf_block_t *block)
{
    return to_block(block_payload(block) + block_size(block) - BLOCK_OVERHEAD);
}

TLSF_INLINE tlsf_block_t *block_link_next(tlsf_block_t *block)
{
    tlsf_block_t *next = block_next(block);
    next->prev = block;
    return next;
}

TLSF_INLINE void block_set_free(tlsf_block_t *block, bool free)
{
    block->header =
        free ? block->header | BLOCK_BIT_FREE : block->header & ~BLOCK_BIT_FREE;
    block_set_prev_free(block_link_next(block), free);
}

TLSF_INLINE size_t adjust_size(size_t size, size_t align)
{
    if (UNLIKELY(size > TLSF_MAX_SIZE))
        return size;
    size = align_up(size, align);
    return size < BLOCK_SIZE_MIN ? BLOCK_SIZE_MIN : size;
}

TLSF_INLINE size_t round_block_size(size_t size)
{
    uint32_t lg = log2floor(size);
    size_t is_large = (size_t) (lg >= (uint32_t) FL_SHIFT);
    uint32_t shift =
        (lg - (uint32_t) SL_SHIFT) & ((uint32_t) (__SIZE_WIDTH__ - 1));
    size_t round = is_large << shift;
    size_t t = round - is_large;
    return (size + t) & ~t;
}

TLSF_INLINE void mapping(size_t size, uint32_t *fl, uint32_t *sl)
{
    uint32_t t = log2floor(size);
    uint32_t small = -(uint32_t) (t < (uint32_t) FL_SHIFT);
    *fl = ~small & (t - (uint32_t) FL_SHIFT + 1);
    uint32_t shift =
        (t - (uint32_t) SL_SHIFT) & ((uint32_t) (__SIZE_WIDTH__ - 1));
    uint32_t sl_large = (uint32_t) (size >> shift) ^ SL_COUNT;
    uint32_t sl_small = (uint32_t) (size >> ALIGN_SHIFT);
    *sl = (~small & sl_large) | (small & sl_small);
}

TLSF_INLINE tlsf_block_t *block_find_suitable(tlsf_t *t,
                                              uint32_t *fl,
                                              uint32_t *sl)
{
    uint32_t sl_map = t->sl[*fl] & (~0U << *sl);
    if (!sl_map) {
        uint32_t fl_map = t->fl & ((*fl + 1 >= 32) ? 0U : (~0U << (*fl + 1)));
        if (UNLIKELY(!fl_map))
            return NULL;
        *fl = bitmap_ffs(fl_map);
        sl_map = t->sl[*fl];
    }
    *sl = bitmap_ffs(sl_map);
    return t->block[*fl][*sl];
}

TLSF_INLINE void remove_free_block(tlsf_t *t,
                                   tlsf_block_t *block,
                                   uint32_t fl,
                                   uint32_t sl)
{
    tlsf_block_t *prev = block->prev_free;
    tlsf_block_t *next = block->next_free;
    next->prev_free = prev;
    prev->next_free = next;
    if (t->block[fl][sl] == block) {
        t->block[fl][sl] = next;
        if (next == &t->block_null) {
            t->sl[fl] &= ~(1U << sl);
            if (!t->sl[fl])
                t->fl &= ~(1U << fl);
        }
    }
}

TLSF_INLINE void insert_free_block(tlsf_t *t,
                                   tlsf_block_t *block,
                                   uint32_t fl,
                                   uint32_t sl)
{
    tlsf_block_t *current = t->block[fl][sl];
    block->next_free = current;
    block->prev_free = &t->block_null;
    current->prev_free = block;
    t->block[fl][sl] = block;
    t->fl |= 1U << fl;
    t->sl[fl] |= 1U << sl;
}

TLSF_INLINE void block_remove(tlsf_t *t, tlsf_block_t *block)
{
    uint32_t fl, sl;
    mapping(block_size(block), &fl, &sl);
    remove_free_block(t, block, fl, sl);
}

TLSF_INLINE void block_insert(tlsf_t *t, tlsf_block_t *block)
{
    uint32_t fl, sl;
    mapping(block_size(block), &fl, &sl);
    insert_free_block(t, block, fl, sl);
}

TLSF_INLINE bool block_can_trim(tlsf_block_t *block, size_t size)
{
    return block_size(block) >= BLOCK_OVERHEAD + BLOCK_SIZE_MIN + size;
}

TLSF_INLINE tlsf_block_t *block_split(tlsf_block_t *block, size_t size)
{
    tlsf_block_t *rest = to_block(block_payload(block) + size - BLOCK_OVERHEAD);
    size_t rest_size = block_size(block) - (size + BLOCK_OVERHEAD);
    rest->header = rest_size;
    block_set_free(rest, true);
    block_set_size(block, size);
    return rest;
}

TLSF_INLINE tlsf_block_t *block_absorb(tlsf_block_t *prev, tlsf_block_t *block)
{
    prev->header += block_size(block) + BLOCK_OVERHEAD;
    block_link_next(prev);
    return prev;
}

TLSF_INLINE tlsf_block_t *block_merge_prev(tlsf_t *t, tlsf_block_t *block)
{
    if (block_is_prev_free(block)) {
        tlsf_block_t *prev = block->prev;
        block_remove(t, prev);
        block = block_absorb(prev, block);
    }
    return block;
}

TLSF_INLINE tlsf_block_t *block_merge_next(tlsf_t *t, tlsf_block_t *block)
{
    tlsf_block_t *next = block_next(block);
    if (block_is_free(next)) {
        block_remove(t, next);
        block = block_absorb(block, next);
    }
    return block;
}

TLSF_INLINE void block_rtrim_free(tlsf_t *t, tlsf_block_t *block, size_t size)
{
    if (!block_can_trim(block, size))
        return;
    tlsf_block_t *rest = block_split(block, size);
    block_link_next(block);
    block_set_prev_free(rest, true);
    block_insert(t, rest);
}

TLSF_INLINE void block_rtrim_used(tlsf_t *t, tlsf_block_t *block, size_t size)
{
    if (!block_can_trim(block, size))
        return;
    tlsf_block_t *rest = block_split(block, size);
    block_set_prev_free(rest, false);
    rest = block_merge_next(t, rest);
    block_insert(t, rest);
}

TLSF_INLINE void *block_use(tlsf_t *t, tlsf_block_t *block, size_t size)
{
    block_rtrim_free(t, block, size);
    block_set_free(block, false);
    return block_payload(block);
}

TLSF_INLINE tlsf_block_t *block_find_free(tlsf_t *t, size_t *size)
{
    size_t req = *size;
    *size = round_block_size(*size);
    uint32_t fl, sl;
    mapping(*size, &fl, &sl);
    tlsf_block_t *block = block_find_suitable(t, &fl, &sl);
    if (UNLIKELY(!block))
        return NULL;
    /* Use the original rounded request size, not the bin minimum.
     * The upstream TLSF sets *size = mapping_size(fl, sl) to keep
     * freed blocks in consistent bins, but when a small request
     * hits a large bin the wasted space is catastrophic (a 48-byte
     * request from an 8 MB block would consume the entire bin-minimum
     * of ~96 KB).  Trimming to the request size instead produces
     * tighter splits and avoids pool exhaustion.
     */
    *size = req;
    remove_free_block(t, block, fl, sl);
    return block;
}

/* -- TLSF API (static pool only) ----------------------------------------- */

static size_t tlsf_pool_init(tlsf_t *t, void *mem, size_t bytes)
{
    if (!t || !mem)
        return 0;

    memset(t, 0, sizeof(*t));
    for (uint32_t i = 0; i < FL_COUNT; i++)
        for (uint32_t j = 0; j < SL_COUNT; j++)
            t->block[i][j] = &t->block_null;

    char *start = align_ptr((char *) mem, ALIGN_SIZE);
    size_t adj = (size_t) (start - (char *) mem);
    if (bytes <= adj)
        return 0;

    size_t pool_bytes = (bytes - adj) & ~(ALIGN_SIZE - 1);
    if (pool_bytes < 2 * BLOCK_OVERHEAD + BLOCK_SIZE_MIN)
        return 0;

    size_t free_size = pool_bytes - 2 * BLOCK_OVERHEAD;
    free_size &= ~(ALIGN_SIZE - 1);
    if (free_size < BLOCK_SIZE_MIN || free_size > BLOCK_SIZE_MAX)
        return 0;

    t->arena = start;

    tlsf_block_t *block = to_block(start - BLOCK_OVERHEAD);
    block->header = free_size | BLOCK_BIT_FREE;
    block_insert(t, block);

    tlsf_block_t *sentinel = block_link_next(block);
    sentinel->header = BLOCK_BIT_PREV_FREE;

    t->size = free_size + 2 * BLOCK_OVERHEAD;
    return free_size;
}

static void *tlsf_malloc(tlsf_t *t, size_t size)
{
    size = adjust_size(size, ALIGN_SIZE);
    if (UNLIKELY(size > TLSF_MAX_SIZE))
        return NULL;

    /* Fast path: small sizes (FL=0) use linear SL mapping directly. */
    if (size < BLOCK_SIZE_SMALL) {
        uint32_t sl = (uint32_t) (size >> ALIGN_SHIFT);
        uint32_t sl_map = t->sl[0] & (~0U << sl);
        if (sl_map) {
            uint32_t found_sl = bitmap_ffs(sl_map);
            size = (size_t) found_sl << ALIGN_SHIFT;
            tlsf_block_t *block = t->block[0][found_sl];
            remove_free_block(t, block, 0, found_sl);
            return block_use(t, block, size);
        }
    }

    tlsf_block_t *block = block_find_free(t, &size);
    if (UNLIKELY(!block))
        return NULL;
    return block_use(t, block, size);
}

static void tlsf_free(tlsf_t *t, void *mem)
{
    if (UNLIKELY(!mem))
        return;
    tlsf_block_t *block = block_from_payload(mem);
    block_set_free(block, true);
    block = block_merge_prev(t, block);
    block = block_merge_next(t, block);
    block_insert(t, block);
}

static void *tlsf_realloc(tlsf_t *t, void *mem, size_t size)
{
    if (UNLIKELY(mem && !size)) {
        tlsf_free(t, mem);
        return NULL;
    }
    if (UNLIKELY(!mem))
        return tlsf_malloc(t, size);

    tlsf_block_t *block = block_from_payload(mem);
    size_t avail = block_size(block);
    size = adjust_size(size, ALIGN_SIZE);
    if (UNLIKELY(size > TLSF_MAX_SIZE))
        return NULL;

    if (size > avail) {
        /* Try in-place forward expansion */
        tlsf_block_t *next = block_next(block);
        if (block_is_free(next) &&
            size <= avail + block_size(next) + BLOCK_OVERHEAD) {
            block_merge_next(t, block);
            block_set_prev_free(block_next(block), false);
        } else {
            void *dst = tlsf_malloc(t, size);
            if (dst) {
                memcpy(dst, mem, avail);
                tlsf_free(t, mem);
            }
            return dst;
        }
    }

    block_rtrim_used(t, block, size);
    return mem;
}

/* -- Mado raw allocator backend ------------------------------------------ */

static uint8_t pool_storage[CONFIG_MEM_TLSF_POOL_SIZE]
    __attribute__((aligned(8)));
static tlsf_t tlsf_instance;
static bool pool_ready;

static bool twin_mem_pool_ensure_ready(void)
{
    if (!pool_ready)
        twin_mem_pool_init();
    return pool_ready;
}

void twin_mem_pool_init(void)
{
    if (pool_ready)
        return;
    size_t usable =
        tlsf_pool_init(&tlsf_instance, pool_storage, sizeof(pool_storage));
    pool_ready = (usable > 0);
}

void *twin_raw_malloc(size_t size)
{
    if (!twin_mem_pool_ensure_ready())
        return NULL;
    void *ptr = tlsf_malloc(&tlsf_instance, size);
    if (!ptr)
        log_error(
            "TLSF pool exhausted (requested %zu bytes, pool %d bytes). "
            "Increase CONFIG_MEM_TLSF_POOL_SIZE.",
            size, CONFIG_MEM_TLSF_POOL_SIZE);
    return ptr;
}

void *twin_raw_calloc(size_t n, size_t size)
{
    if (n && size > SIZE_MAX / n)
        return NULL;
    if (!twin_mem_pool_ensure_ready())
        return NULL;
    size_t total = n * size;
    void *ptr = tlsf_malloc(&tlsf_instance, total);
    if (ptr)
        memset(ptr, 0, total);
    return ptr;
}

void *twin_raw_realloc(void *ptr, size_t size)
{
    if (!ptr && !twin_mem_pool_ensure_ready())
        return NULL;
    return tlsf_realloc(&tlsf_instance, ptr, size);
}

void twin_raw_free(void *ptr)
{
    tlsf_free(&tlsf_instance, ptr);
}

#endif /* CONFIG_MEM_TLSF */
