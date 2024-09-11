/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */
#include <stdlib.h>

#include "twin.h"

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

void twin_animation_advance_frame(twin_animation_t *anim)
{
    if (!anim)
        return;
    twin_animation_iter_advance(anim->iter);
}

void twin_animation_destroy(twin_animation_t *anim)
{
    if (!anim)
        return;

    free(anim->iter);
    for (twin_count_t i = 0; i < anim->n_frames; i++) {
        twin_pixmap_destroy(anim->frames[i]);
    }
    free(anim->frames);
    free(anim->frame_delays);
    free(anim);
}

twin_animation_iter_t *twin_animation_iter_init(twin_animation_t *anim)
{
    twin_animation_iter_t *iter = malloc(sizeof(twin_animation_iter_t));
    if (!iter || !anim)
        return NULL;
    iter->current_index = 0;
    iter->current_frame = anim->frames[0];
    iter->current_delay = anim->frame_delays[0];
    anim->iter = iter;
    iter->anim = anim;
    return iter;
}

void twin_animation_iter_advance(twin_animation_iter_t *iter)
{
    twin_animation_t *anim = iter->anim;
    iter->current_index++;
    if (iter->current_index >= anim->n_frames) {
        if (anim->loop) {
            iter->current_index = 0;
        } else {
            iter->current_index = anim->n_frames - 1;
        }
    }
    iter->current_frame = anim->frames[iter->current_index];
    iter->current_delay = anim->frame_delays[iter->current_index];
}
