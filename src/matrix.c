/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include "twin_private.h"

void twin_matrix_multiply(twin_matrix_t *result,
                          const twin_matrix_t *a,
                          const twin_matrix_t *b)
{
    twin_matrix_t r;

    for (int row = 0; row < 3; row++) {
        twin_fixed_t t;
        for (int col = 0; col < 2; col++) {
            if (row == 2)
                t = b->m[2][col];
            else
                t = 0;
            for (int n = 0; n < 2; n++)
                t += twin_fixed_mul(a->m[row][n], b->m[n][col]);
            r.m[row][col] = t;
        }
    }

    *result = r;
}

void twin_matrix_identity(twin_matrix_t *m)
{
    m->m[0][0] = TWIN_FIXED_ONE;
    m->m[0][1] = 0;
    m->m[1][0] = 0;
    m->m[1][1] = TWIN_FIXED_ONE;
    m->m[2][0] = 0;
    m->m[2][1] = 0;
}

bool twin_matrix_is_identity(twin_matrix_t *m)
{
    return m->m[0][0] == TWIN_FIXED_ONE && m->m[0][1] == 0 && m->m[1][0] == 0 &&
           m->m[1][1] == TWIN_FIXED_ONE && m->m[2][0] == 0 && m->m[2][1] == 0;
}

void twin_matrix_translate(twin_matrix_t *m, twin_fixed_t tx, twin_fixed_t ty)
{
    twin_matrix_t t;

    t.m[0][0] = TWIN_FIXED_ONE;
    t.m[0][1] = 0;
    t.m[1][0] = 0;
    t.m[1][1] = TWIN_FIXED_ONE;
    t.m[2][0] = tx;
    t.m[2][1] = ty;
    twin_matrix_multiply(m, &t, m);
}

void twin_matrix_scale(twin_matrix_t *m, twin_fixed_t sx, twin_fixed_t sy)
{
    twin_matrix_t t;

    t.m[0][0] = sx;
    t.m[0][1] = 0;
    t.m[1][0] = 0;
    t.m[1][1] = sy;
    t.m[2][0] = 0;
    t.m[2][1] = 0;
    twin_matrix_multiply(m, &t, m);
}

twin_fixed_t _twin_matrix_determinant(twin_matrix_t *matrix)
{
    twin_fixed_t a, b, c, d;

    a = matrix->m[0][0];
    b = matrix->m[0][1];
    c = matrix->m[1][0];
    d = matrix->m[1][1];

    twin_fixed_t det = twin_fixed_mul(a, d) - twin_fixed_mul(b, c);

    return det;
}

twin_point_t _twin_matrix_expand(twin_matrix_t *matrix)
{
    twin_fixed_t a = matrix->m[0][0];
    twin_fixed_t d = matrix->m[1][1];
    twin_fixed_t aa = twin_fixed_mul(a, a);
    twin_fixed_t dd = twin_fixed_mul(d, d);
    twin_point_t expand;

    expand.x = twin_fixed_sqrt(aa + dd);
    expand.y = twin_fixed_div(_twin_matrix_determinant(matrix), expand.x);
    return expand;
}

void twin_matrix_rotate(twin_matrix_t *m, twin_angle_t a)
{
    twin_fixed_t c, s;
    twin_sincos(a, &s, &c);

    twin_matrix_t t = {
        .m[0][0] = c,
        .m[0][1] = s,
        .m[1][0] = -s,
        .m[1][1] = c,
        .m[2][0] = 0,
        .m[2][1] = 0,
    };
    twin_matrix_multiply(m, &t, m);
}

twin_sfixed_t _twin_matrix_x(twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y)
{
    twin_sfixed_t s =
        twin_fixed_to_sfixed(twin_fixed_mul(m->m[0][0], x) +
                             twin_fixed_mul(m->m[1][0], y) + m->m[2][0]);
    return s;
}

twin_sfixed_t _twin_matrix_y(twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y)
{
    twin_sfixed_t s =
        twin_fixed_to_sfixed(twin_fixed_mul(m->m[0][1], x) +
                             twin_fixed_mul(m->m[1][1], y) + m->m[2][1]);
    return s;
}

twin_fixed_t _twin_matrix_fx(twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y)
{
    return twin_fixed_mul(m->m[0][0], x) + twin_fixed_mul(m->m[1][0], y) +
           m->m[2][0];
}

twin_fixed_t _twin_matrix_fy(twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y)
{
    return twin_fixed_mul(m->m[0][1], x) + twin_fixed_mul(m->m[1][1], y) +
           m->m[2][1];
}

twin_sfixed_t _twin_matrix_dx(twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y)
{
    return twin_fixed_to_sfixed(twin_fixed_mul(m->m[0][0], x) +
                                twin_fixed_mul(m->m[1][0], y));
}

twin_sfixed_t _twin_matrix_dy(twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y)
{
    return twin_fixed_to_sfixed(twin_fixed_mul(m->m[0][1], x) +
                                twin_fixed_mul(m->m[1][1], y));
}

twin_sfixed_t _twin_matrix_len(twin_matrix_t *m,
                               twin_fixed_t dx,
                               twin_fixed_t dy)
{
    twin_fixed_t xs =
        (twin_fixed_mul(m->m[0][0], dx) + twin_fixed_mul(m->m[1][0], dy));
    twin_fixed_t ys =
        (twin_fixed_mul(m->m[0][1], dx) + twin_fixed_mul(m->m[1][1], dy));
    twin_fixed_t ds = (twin_fixed_mul(xs, xs) + twin_fixed_mul(ys, ys));
    return (twin_fixed_to_sfixed(twin_fixed_sqrt(ds)));
}
