/* Math microbenchmarks. Compile separately from math-bench-kernels.c, no LTO.
 * Inputs fit both backends. Each call measures N independent operations.
 */
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "twin.h"

#define N 1024
#define SAMPLES 7
#define TARGET_NS UINT64_C(20000000)
typedef int64_t wide_t;
#define BINARY(name, type) \
    void math_bench_##name(const type *, const type *, type *, size_t)
#define UNARY(name, type) void math_bench_##name(const type *, type *, size_t)
BINARY(mul, twin_fixed_t);
BINARY(div, twin_fixed_t);
UNARY(sqrt, twin_fixed_t);
UNARY(sincos, twin_fixed_t);
UNARY(acos, twin_fixed_t);
BINARY(wmul, wide_t);
BINARY(wdiv, wide_t);
UNARY(wsqrt, wide_t);
BINARY(compose, twin_matrix_t);
UNARY(translate, twin_matrix_t);
UNARY(scale, twin_matrix_t);
UNARY(rotate, twin_matrix_t);
void math_bench_transform(const twin_matrix_t *,
                          const twin_fixed_t *,
                          const twin_fixed_t *,
                          twin_fixed_t *,
                          size_t);
static twin_fixed_t a[N], b[N], small[N], whole[N], angles[N], unit[N];
static twin_fixed_t out[N * 2];
static wide_t wa[N], wb[N], wwhole[N], wout[N];
static twin_matrix_t ma[N], mb[N], mout[N];
static volatile uint64_t sink;

static uint32_t next(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    return x ^ (x << 5);
}

static uint64_t now(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t))
        abort();
    return (uint64_t) t.tv_sec * UINT64_C(1000000000) + t.tv_nsec;
}

static int cmp(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *) a, y = *(const uint64_t *) b;
    return (x > y) - (x < y);
}

static void measure(const char *name, void (*run)(void))
{
    uint64_t start, elapsed, samples[SAMPLES];
    unsigned reps = 1;
    /* Calibration also warms caches and the ondemand CPU frequency governor. */
    do {
        start = now();
        for (unsigned i = 0; i < reps; i++)
            run();
        elapsed = now() - start;
        if (elapsed >= TARGET_NS)
            break;
        reps *= 2;
    } while (reps < (1u << 24));
    for (unsigned s = 0; s < SAMPLES; s++) {
        start = now();
        for (unsigned i = 0; i < reps; i++)
            run();
        samples[s] = now() - start;
    }
    qsort(samples, SAMPLES, sizeof(*samples), cmp);
    /* Integer nanoseconds avoid involving FP even in the timing driver. */
    printf("%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n", name,
           (uint64_t) reps * N, samples[0], samples[SAMPLES / 2],
           samples[SAMPLES - 1]);
    fflush(stdout);
}

#define SCALAR_BINARY(label, kernel, x, y, result) \
    static void run_##label(void)                  \
    {                                              \
        math_bench_##kernel(x, y, result, N);      \
        sink ^= (uint64_t) result[N - 1];          \
    }
#define SCALAR_UNARY(label, kernel, x, result) \
    static void run_##label(void)              \
    {                                          \
        math_bench_##kernel(x, result, N);     \
        sink ^= (uint64_t) result[N - 1];      \
    }
SCALAR_BINARY(mul, mul, a, b, out)
SCALAR_BINARY(div, div, a, b, out)
SCALAR_BINARY(div_small, div, small, b, out)
SCALAR_BINARY(div_whole, div, a, whole, out)
SCALAR_UNARY(sqrt, sqrt, a, out)
SCALAR_UNARY(sincos, sincos, angles, out)
SCALAR_UNARY(acos, acos, unit, out)
SCALAR_BINARY(wmul, wmul, wa, wb, wout)
SCALAR_BINARY(wdiv, wdiv, wa, wb, wout)
SCALAR_BINARY(wdiv_whole, wdiv, wa, wwhole, wout)
SCALAR_UNARY(wsqrt, wsqrt, wa, wout)
static void run_compose(void)
{
    math_bench_compose(ma, mb, mout, N);
    sink ^= (uint32_t) mout[N - 1].m[2][0];
}
#define MATRIX_UNARY(label)                     \
    static void run_##label(void)               \
    {                                           \
        math_bench_##label(ma, mout, N);        \
        sink ^= (uint32_t) mout[N - 1].m[0][0]; \
    }
MATRIX_UNARY(translate)
MATRIX_UNARY(scale)
MATRIX_UNARY(rotate)
static void run_transform(void)
{
    math_bench_transform(ma, small, b, out, N);
    sink ^= (uint32_t) out[N - 1];
}

int main(int argc, char **argv)
{
    if (argc > 2)
        return 2;
    uint32_t seed = 42;
    for (int i = 0; i < N; i++) {
        seed = next(seed);
        a[i] = (seed & 0xfffff) + 1;
        small[i] = (int32_t) (seed & 0xffff) - 32768;
        angles[i] = seed & 4095;
        unit[i] = (int32_t) (seed & 0x1ffff) - 65536;
        seed = next(seed);
        b[i] = (seed & 0xfffff) + 65536;
        whole[i] = ((seed & 15) + 1) * 65536;
        wa[i] = (wide_t) a[i] * 65536 + (seed & 0xffff);
        wb[i] = (wide_t) b[i] * 65536 + (seed & 0xffff);
        wwhole[i] = ((wide_t) (seed & 15) + 1) * INT64_C(4294967296);
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 2; c++) {
                seed = next(seed);
                ma[i].m[r][c] = (int32_t) (seed & 0x1ffff) - 65536;
                seed = next(seed);
                mb[i].m[r][c] = (int32_t) (seed & 0x1ffff) - 65536;
            }
    }
    puts("kernel,operations,min_ns,median_ns,max_ns");
    int measured = 0;
#define MEASURE(name)                               \
    do {                                            \
        if (argc == 1 || !strcmp(argv[1], #name)) { \
            measure(#name, run_##name);             \
            ++measured;                             \
        }                                           \
    } while (0)
    MEASURE(mul);
    MEASURE(div);
    MEASURE(div_small);
    MEASURE(div_whole);
    MEASURE(sqrt);
    MEASURE(sincos);
    MEASURE(acos);
    MEASURE(wmul);
    MEASURE(wdiv);
    MEASURE(wdiv_whole);
    MEASURE(wsqrt);
    MEASURE(compose);
    MEASURE(translate);
    MEASURE(scale);
    MEASURE(rotate);
    MEASURE(transform);
    return measured ? 0 : 2;
}
