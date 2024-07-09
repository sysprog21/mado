/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twinint.h"

twin_dfixed_t
_twin_distance_to_point_squared (twin_spoint_t *a, twin_spoint_t *b)
{
    twin_dfixed_t dx = (b->x - a->x);
    twin_dfixed_t dy = (b->y - a->y);

    return dx*dx + dy*dy;
}

twin_dfixed_t
_twin_distance_to_line_squared (twin_spoint_t *p, twin_spoint_t *p1, twin_spoint_t *p2)
{
    /*
     * Convert to normal form (AX + BY + C = 0)
     *
     * (X - x1) * (y2 - y1) = (Y - y1) * (x2 - x1)
     *
     * X * (y2 - y1) - Y * (x2 - x1) - x1 * (y2 - y1) + y1 * (x2 - x1) = 0
     *
     * A = (y2 - y1)
     * B = (x1 - x2)
     * C = (y1x2 - x1y2)
     *
     * distance² = (AX + BC + C)² / (A² + B²)
     */
    twin_dfixed_t   A = p2->y - p1->y;
    twin_dfixed_t   B = p1->x - p2->x;
    twin_dfixed_t   C = ((twin_dfixed_t) p1->y * p2->x - 
			 (twin_dfixed_t) p1->x * p2->y);
    twin_dfixed_t   den, num;

    num = A * p->x + B * p->y + C;
    if (num < 0)
	num = -num;
    den = A * A + B * B;
    if (den == 0 || num >= 0x8000)
	return _twin_distance_to_point_squared (p, p1);
    else
	return (num * num) / den;
}

