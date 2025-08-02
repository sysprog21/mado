/*
 * Twin - A Tiny Window System
 * Copyright (c) 2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/* Closure lifetime management implementation
 *
 * The Mado window system uses an asynchronous event model where widgets
 * schedule work items and timeouts with themselves as closure pointers. When a
 * widget is destroyed, these queued items may still reference the freed memory,
 * leading to use-after-free vulnerabilities and crashes.
 *
 * This implements a reference-counting closure tracking system that:
 * 1. Maintains a registry of all active closure pointers
 * 2. Tracks reference counts for each closure
 * 3. Provides validation before closure use
 * 4. Supports marking closures as "being freed" to prevent new references
 *
 * Flow Control
 * ------------
 * 1. Registration: When a widget/object is created, it registers itself as a
 * closure
 * 2. Reference: When scheduling work/timeout, the system adds a reference
 * 3. Validation: Before executing callbacks, the system validates the closure
 * 4. Cleanup: When work completes or widget is destroyed, references are
 * removed
 *
 * Important: Closures must be registered before they can be used in
 * work/timeout systems. The typical pattern is:
 *   1. Object creation: _twin_closure_register(obj)
 *   2. Schedule work: twin_set_work() calls _twin_closure_ref()
 *   3. Object destruction: _twin_closure_mark_for_free() then
 * _twin_closure_unregister()
 *   4. Work execution: _twin_closure_is_valid() check prevents use-after-free
 */

#include <string.h>

#include "twin_private.h"

/* Global closure tracker instance */
twin_closure_tracker_t _twin_closure_tracker = {0};

/*
 * Initialize the closure tracking system.
 * Called once during screen initialization to set up the tracking table.
 */
void _twin_closure_tracker_init(void)
{
    memset(&_twin_closure_tracker, 0, sizeof(_twin_closure_tracker));
    _twin_closure_tracker.initialized = true;
    /* TODO: Initialize mutex when threading support is added */
}

/*
 * Find an entry for a given closure pointer.
 * Uses linear search which is efficient for small closure counts.
 *
 * Returns entry pointer if found, NULL otherwise
 */
static twin_closure_entry_t *_twin_closure_find_entry(void *closure)
{
    if (!closure)
        return NULL;

    /* Quick rejection of obviously invalid pointers */
    if (!twin_pointer_valid(closure))
        return NULL;

    for (int i = 0; i < _twin_closure_tracker.count; i++) {
        if (_twin_closure_tracker.entries[i].closure == closure)
            return &_twin_closure_tracker.entries[i];
    }
    return NULL;
}

/*
 * Register a closure pointer with the tracking system.
 * If already registered, increments the reference count.
 *
 * This is typically called when a widget/object is created and may be used
 * as a closure in work items or timeouts.
 * @closure : The pointer to track (usually a widget or toplevel)
 *
 * Returns true if successfully registered, false on failure
 */
bool _twin_closure_register(void *closure)
{
    if (!closure)
        return false;

    /* Quick rejection of obviously invalid pointers */
    if (!twin_pointer_valid(closure))
        return false;

    /* If tracker not initialized, skip registration */
    if (!_twin_closure_tracker.initialized)
        return true; /* Pretend success if tracking not yet enabled */

    /* Check if already registered */
    twin_closure_entry_t *entry = _twin_closure_find_entry(closure);
    if (entry) {
        /* Already registered, just increment ref count */
        entry->ref_count++;
        return true;
    }

    /* Check capacity */
    if (_twin_closure_tracker.count >= TWIN_MAX_CLOSURES)
        return false;

    /* Add new entry at the end */
    entry = &_twin_closure_tracker.entries[_twin_closure_tracker.count++];
    entry->closure = closure;
    entry->ref_count = 1;
    entry->marked_for_free = false;

    return true;
}

/*
 * Remove a closure from the tracking system.
 * Called during object destruction to clean up tracking entries.
 *
 * Uses swap-with-last removal for O(1) deletion without gaps.
 */
void _twin_closure_unregister(void *closure)
{
    if (!closure)
        return;

    for (int i = 0; i < _twin_closure_tracker.count; i++) {
        if (_twin_closure_tracker.entries[i].closure == closure) {
            /* Swap with last entry and decrement count */
            if (i < _twin_closure_tracker.count - 1) {
                _twin_closure_tracker.entries[i] =
                    _twin_closure_tracker
                        .entries[_twin_closure_tracker.count - 1];
            }
            _twin_closure_tracker.count--;
            return;
        }
    }
}

/*
 * Increment the reference count for a closure.
 * Called when scheduling new work items or timeouts.
 *
 * Fails if closure is not registered or marked for deletion.
 * This prevents new references to objects being destroyed.
 */
bool _twin_closure_ref(void *closure)
{
    /* Skip if tracker not initialized */
    if (!_twin_closure_tracker.initialized)
        return true;

    twin_closure_entry_t *entry = _twin_closure_find_entry(closure);
    if (!entry || entry->marked_for_free)
        return false;

    entry->ref_count++;
    return true;
}

/*
 * Decrement the reference count for a closure.
 * Called when work items complete or timeouts are cleared.
 *
 * Note: We don't auto-unregister at zero refs to maintain explicit
 * ownership semantics. The owner must call unregister during destruction.
 */
bool _twin_closure_unref(void *closure)
{
    /* Skip if tracker not initialized */
    if (!_twin_closure_tracker.initialized)
        return true;

    twin_closure_entry_t *entry = _twin_closure_find_entry(closure);
    if (!entry)
        return false;

    if (entry->ref_count > 0)
        entry->ref_count--;

    return true;
}

/*
 * Validate a closure pointer before use.
 * This is the critical safety check called before executing callbacks.
 *
 * Validation steps:
 * 1. NULL check
 * 2. Platform-specific pointer validity (checks for obviously bad addresses)
 * 3. Presence in tracking table (untracked = invalid)
 * 4. Not marked for deletion
 * 5. Has active references
 *
 * Returns: true if safe to use, false if potentially freed
 */
bool _twin_closure_is_valid(void *closure)
{
    if (!closure)
        return false;

    /* First-line defense: basic pointer sanity */
    if (!twin_pointer_valid(closure))
        return false;

    /* If tracker not initialized, fall back to basic pointer validation */
    if (!_twin_closure_tracker.initialized)
        return true; /* Assume valid if tracking not yet enabled */

    /* Must be tracked to be valid */
    twin_closure_entry_t *entry = _twin_closure_find_entry(closure);
    if (!entry)
        return false;

    /* Marked closures are in process of being freed */
    if (entry->marked_for_free)
        return false;

    /* Must have active references */
    return entry->ref_count > 0;
}

/*
 * Mark a closure as being freed.
 * Called at the start of object destruction to prevent races.
 *
 * Once marked:
 * - No new references can be added (ref() will fail)
 * - Existing references remain valid until cleared
 * - is_valid() returns false to prevent new callback execution
 */
void _twin_closure_mark_for_free(void *closure)
{
    twin_closure_entry_t *entry = _twin_closure_find_entry(closure);
    if (entry)
        entry->marked_for_free = true;
}
