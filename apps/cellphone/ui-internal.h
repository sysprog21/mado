/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Helpers shared by the UI kit's own files (ui.c, ui-paint.c, ui-input.c);
 * apps use phone.h only.
 */

#ifndef _UI_INTERNAL_H_
#define _UI_INTERNAL_H_

#include "phone.h"

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

/* Font Awesome code points live in U+F000..U+F8FF: UTF-8 0xEF 0x80.. */
static inline bool _is_symbol(const char *s)
{
    return s && (uint8_t) s[0] == 0xef && (uint8_t) s[1] >= 0x80 &&
           (uint8_t) s[1] <= 0xa3;
}

#endif /* _UI_INTERNAL_H_ */
