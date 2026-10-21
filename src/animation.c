/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */
#include <stdlib.h>

#include "twin_private.h"

twin_time_t twin_animation_get_current_delay(const twin_animation_t *anim)
{
    return anim ? anim->delay : 0;
}

twin_pixmap_t *twin_animation_get_current_frame(const twin_animation_t *anim)
{
    return anim ? anim->pixmap : NULL;
}

void twin_animation_advance_frame(twin_animation_t *anim)
{
    if (!anim)
        return;
    /* The frame is drawn into the pixmap in place: show it where shown,
     * unless the animation has played out and drew nothing */
    if (anim->advance(anim))
        twin_pixmap_damage(anim->pixmap, 0, 0, anim->pixmap->width,
                           anim->pixmap->height);
}

void _twin_animation_destroy(twin_animation_t *anim)
{
    if (anim)
        anim->close(anim);
}
