/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */
#include <stdlib.h>

#include "twin_private.h"

twin_time_t twin_animation_get_current_delay(const twin_animation_t *anim)
{
    if (!anim)
        return 0;
    return anim->iter->current_delay;
}

twin_pixmap_t *twin_animation_get_current_frame(const twin_animation_t *anim)
{
    if (!anim)
        return NULL;
    return anim->iter->current_frame;
}

static void _twin_animation_iter_advance(twin_animation_iter_t *iter)
{
    twin_animation_t *anim = iter->anim;
    iter->current_index++;
    if (iter->current_index >= anim->n_frames)
        iter->current_index = anim->loop ? 0 : anim->n_frames - 1;
    iter->current_frame = anim->frames[iter->current_index];
    iter->current_delay = anim->frame_delays[iter->current_index];
}

void twin_animation_advance_frame(twin_animation_t *anim)
{
    if (!anim)
        return;
    _twin_animation_iter_advance(anim->iter);
}

void _twin_animation_destroy(twin_animation_t *anim)
{
    if (!anim)
        return;

    twin_free(anim->iter);
    for (twin_count_t i = 0; i < anim->n_frames; i++)
        twin_pixmap_destroy(anim->frames[i]);
    twin_free(anim->frames);
    twin_free(anim->frame_delays);
    twin_free(anim);
}

twin_animation_iter_t *_twin_animation_iter_init(twin_animation_t *anim)
{
    if (!anim || !anim->n_frames)
        return NULL;
    twin_animation_iter_t *iter = twin_malloc(sizeof(twin_animation_iter_t));
    if (!iter)
        return NULL;
    iter->current_index = 0;
    iter->current_frame = anim->frames[0];
    iter->current_delay = anim->frame_delays[0];
    anim->iter = iter;
    iter->anim = anim;
    return iter;
}
