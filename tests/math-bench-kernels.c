/* Batched kernels keep timing/formatting out of the measured math code. */
#include "twin_private.h"

void math_bench_mul(const twin_fixed_t *a,
                    const twin_fixed_t *b,
                    twin_fixed_t *out,
                    size_t n)
{
    for (size_t i = 0; i < n; i++)
        out[i] = twin_fixed_mul(a[i], b[i]);
}

void math_bench_div(const twin_fixed_t *a,
                    const twin_fixed_t *b,
                    twin_fixed_t *out,
                    size_t n)
{
    for (size_t i = 0; i < n; i++)
        out[i] = twin_fixed_div(a[i], b[i]);
}

void math_bench_sqrt(const twin_fixed_t *a, twin_fixed_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++)
        out[i] = twin_fixed_sqrt(a[i]);
}

void math_bench_sincos(const twin_fixed_t *a, twin_fixed_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        twin_fixed_t s, c;
        twin_sincos(a[i], &s, &c);
        out[i] = s ^ c;
    }
}

void math_bench_acos(const twin_fixed_t *a, twin_fixed_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++)
        out[i] = twin_acos(a[i]);
}

void math_bench_wmul(const twin_xfixed_t *a,
                     const twin_xfixed_t *b,
                     twin_xfixed_t *out,
                     size_t n)
{
    for (size_t i = 0; i < n; i++)
        out[i] = twin_xfixed_mul(a[i], b[i]);
}

void math_bench_wdiv(const twin_xfixed_t *a,
                     const twin_xfixed_t *b,
                     twin_xfixed_t *out,
                     size_t n)
{
    for (size_t i = 0; i < n; i++)
        out[i] = twin_xfixed_div(a[i], b[i]);
}

void math_bench_wsqrt(const twin_xfixed_t *a, twin_xfixed_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++)
        out[i] = _twin_xfixed_sqrt(a[i]);
}

void math_bench_compose(const twin_matrix_t *a,
                        const twin_matrix_t *b,
                        twin_matrix_t *out,
                        size_t n)
{
    for (size_t i = 0; i < n; i++)
        twin_matrix_multiply(&out[i], &a[i], &b[i]);
}

void math_bench_translate(const twin_matrix_t *a, twin_matrix_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        out[i] = a[i];
        twin_matrix_translate(&out[i], 32768, -16384);
    }
}

void math_bench_scale(const twin_matrix_t *a, twin_matrix_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        out[i] = a[i];
        twin_matrix_scale(&out[i], 81920, -32768);
    }
}

void math_bench_rotate(const twin_matrix_t *a, twin_matrix_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        out[i] = a[i];
        twin_matrix_rotate(&out[i], (twin_angle_t) (i * 17));
    }
}

void math_bench_transform(const twin_matrix_t *a,
                          const twin_fixed_t *x,
                          const twin_fixed_t *y,
                          twin_fixed_t *out,
                          size_t n)
{
    for (size_t i = 0; i < n; i++)
        twin_matrix_transform(&a[i], x[i], y[i], &out[2 * i], &out[2 * i + 1]);
}
