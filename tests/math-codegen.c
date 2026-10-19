/* Freestanding probes for ISA code generation and QEMU execution. */
#include "twin.h"

#include "math-gen.h"

twin_fixed_t math_product(twin_fixed_t a, twin_fixed_t b)
{
    return twin_fixed_mul(a, b);
}

twin_fixed_t math_quotient(twin_fixed_t a, twin_fixed_t b)
{
    return twin_fixed_div(a, b);
}

void math_compose(twin_matrix_t *r,
                  const twin_matrix_t *a,
                  const twin_matrix_t *b)
{
    twin_math_compose(r, a, b);
}

void math_translate(twin_matrix_t *m, twin_fixed_t tx, twin_fixed_t ty)
{
    twin_math_translate(m, tx, ty);
}

void math_scale(twin_matrix_t *m, twin_fixed_t sx, twin_fixed_t sy)
{
    twin_math_scale(m, sx, sy);
}

void math_rotate(twin_matrix_t *m, twin_fixed_t c, twin_fixed_t s)
{
    twin_math_rotate(m, c, s);
}

#if TWIN_MATH_FLOAT
float math_sqrt(float value)
{
    return twin_math_sqrt_positive(value);
}
#endif

#ifdef MADO_MATH_KERNEL_TEST
#include "math-reference.h"
twin_fixed_t twin_fixed_sqrt(twin_fixed_t value);
#endif

#ifdef MADO_MATH_FREESTANDING_TEST
#ifndef MADO_MATH_KERNEL_TEST
/* Without src/fixed.c linked in, supply the backend tag it would define. */
const char TWIN_MATH_ABI = 0;
#endif
/* Only for this libc-free test: RV32I needs a multiply runtime. The production
 * build links the compiler runtime. This is not used for performance claims.
 */
#if !defined(__riscv_mul)
uint32_t __mulsi3(uint32_t a, uint32_t b)
{
    uint32_t result = 0;
    while (b) {
        if (b & 1)
            result += a;
        a <<= 1;
        b >>= 1;
    }
    return result;
}

uint64_t __muldi3(uint64_t a, uint64_t b)
{
    uint64_t result = 0;
    while (b) {
        if (b & 1)
            result += a;
        a <<= 1;
        b >>= 1;
    }
    return result;
}
#endif

static uint32_t next(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    return x ^ (x << 5);
}

#if TWIN_MATH_FLOAT
/* Portable C reference, deliberately independent of the RISC-V ASM path. */
static twin_fixed_t convert(float value, int down)
{
    if (value >= 0x1p31f)
        return INT32_MAX;
    if (value <= -0x1p31f)
        return INT32_MIN;
    twin_fixed_t result = (twin_fixed_t) value;
    if (down && (float) result > value)
        --result;
    return result;
}
#endif

static int run(void)
{
#if TWIN_MATH_FLOAT
    for (int n = 0; n <= 1024; n++) {
        float x = n;
        if (math_sqrt(x * x) != x)
            return 6;
    }
    if (math_sqrt(0.25f) != 0.5f)
        return 6;
#endif
#ifdef MADO_MATH_KERNEL_TEST
    uint32_t hash = 2166136261u;
    for (int angle = 0; angle < TWIN_ANGLE_360; angle++) {
        twin_fixed_t s, c;
        twin_sincos(angle, &s, &c);
#if TWIN_MATH_FLOAT
        const int bound = 2;
#elif defined(CONFIG_MATH_TRIG_TABLE) && CONFIG_MATH_TRIG_TABLE
        const int bound = 1;
#else
        const int bound = 6;
#endif
        /* The reference truncates double sine to Q16, adding <1 LSB error. */
        int error_s = s - reference_sine[angle];
        int error_c = c - reference_sine[(angle + TWIN_ANGLE_90) & 4095];
        if (error_s < -bound || error_s > bound || error_c < -bound ||
            error_c > bound)
            return 7;
        hash = (hash ^ (uint32_t) s) * 16777619u;
        hash = (hash ^ (uint32_t) c) * 16777619u;
    }
#if !TWIN_MATH_FLOAT
#if defined(CONFIG_MATH_TRIG_TABLE) && CONFIG_MATH_TRIG_TABLE
    if (hash != 0xd762c0fdu)
#else
    if (hash != 0xe3ecd53du)
#endif
        return 7;
#endif
    if (twin_fixed_sqrt(-1) != 0 || twin_fixed_sqrt(0) != 0 ||
        twin_fixed_sqrt(4 * TWIN_FIXED_ONE) != 2 * TWIN_FIXED_ONE)
        return 8;
#endif
    uint32_t state = 123;
    for (int i = 0; i < 10000; i++) {
        state = next(state);
        twin_fixed_t a = (int32_t) state;
        state = next(state);
        twin_fixed_t b = (int32_t) state;
#if TWIN_MATH_FLOAT
        /* Full-range operands mostly saturate; shifted ones stay in range
         * and exercise round-down conversion.
         */
        float product = (float) a * ((float) b * 0x1p-16f);
        if (math_product(a, b) != convert(product, 1))
            return 1;
        product = (float) (a >> 8) * ((float) (b >> 8) * 0x1p-16f);
        if (math_product(a >> 8, b >> 8) != convert(product, 1))
            return 1;
        if (b) {
            float quotient = ((float) a / (float) b) * 0x1p16f;
            if (math_quotient(a, b) != convert(quotient, 0))
                return 2;
        }
#else
        /* Products here fit in the coordinate range. */
        a >>= 8;
        b >>= 8;
        twin_fixed_t expected = (twin_fixed_t) (((int64_t) a * b) >> 16);
        if (math_product(a, b) != expected)
            return 1;
        if (twin_fixed_div(a, 2 * TWIN_FIXED_ONE) != a / 2)
            return 2;
#endif
    }
    if (math_product(INT32_MIN, TWIN_FIXED_ONE) != INT32_MIN ||
        math_product(INT32_MAX, TWIN_FIXED_ONE) != INT32_MAX ||
        math_product(-1, 1) != -1)
        return 3;
    twin_matrix_t a = {{{65536, 32768}, {-16384, 65536}, {8192, -4096}}};
    twin_matrix_t b = {{{32768, -65536}, {65536, 16384}, {-8192, 12288}}};
    twin_matrix_t expected, actual;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 2; c++) {
            twin_fixed_t value = r == 2 ? b.m[2][c] : 0;
            for (int n = 0; n < 2; n++)
                value += twin_fixed_mul(a.m[r][n], b.m[n][c]);
            expected.m[r][c] = value;
        }
    }
    math_compose(&actual, &a, &b);
    math_compose(&b, &a, &b);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 2; c++)
            if (actual.m[r][c] != expected.m[r][c] ||
                b.m[r][c] != expected.m[r][c])
                return 4;
    for (int operation = 0; operation < 3; operation++) {
        twin_matrix_t t = {{{65536, 0}, {0, 65536}, {0, 0}}};
        actual = a;
        if (operation == 0) {
            t.m[2][0] = 32768;
            t.m[2][1] = -16384;
            math_translate(&actual, 32768, -16384);
        } else if (operation == 1) {
            t.m[0][0] = 81920;
            t.m[1][1] = -32768;
            math_scale(&actual, 81920, -32768);
        } else {
            t.m[0][0] = t.m[1][1] = 32768;
            t.m[0][1] = -32768;
            t.m[1][0] = 32768;
            math_rotate(&actual, 32768, -32768);
        }
        math_compose(&expected, &t, &a);
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 2; c++)
                if (actual.m[r][c] != expected.m[r][c])
                    return 5;
    }
    return 0;
}

void _start(void)
{
    register int status __asm__("a0") = run();
    register int syscall __asm__("a7") = 93;
    __asm__ volatile("ecall" : : "r"(status), "r"(syscall) : "memory");
    __builtin_unreachable();
}
#endif
