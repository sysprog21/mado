/* Arithmetic policy and generated affine composition regression tests. */
#include <assert.h>
#include <math.h>
#include "twin_private.h"

static void close_fixed(twin_fixed_t actual, double expected)
{
    assert(fabs((double) actual / 65536.0 - expected) < 0.002);
}

static uint32_t next_random(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    return x ^ (x << 5);
}

static void check_scalar_pair(twin_fixed_t a, twin_fixed_t b)
{
#if TWIN_MATH_FLOAT
    float product = (float) a * ((float) b * 0x1p-16f);
    twin_fixed_t expected;
    if (product >= 0x1p31f)
        expected = INT32_MAX;
    else if (product <= -0x1p31f)
        expected = INT32_MIN;
    else
        expected = (twin_fixed_t) floorf(product);
    assert(twin_fixed_mul(a, b) == expected);
    if (b) {
        float quotient = ((float) a / (float) b) * 0x1p16f;
        if (quotient >= 0x1p31f)
            expected = INT32_MAX;
        else if (quotient <= -0x1p31f)
            expected = INT32_MIN;
        else
            expected = (twin_fixed_t) quotient;
        assert(twin_fixed_div(a, b) == expected);
    }
#else
    int64_t product = ((int64_t) a * b) >> 16;
    if (product >= INT32_MIN && product <= INT32_MAX)
        assert(twin_fixed_mul(a, b) == product);
    if (b) {
        int64_t quotient = ((int64_t) a * 65536) / b;
        if (quotient >= INT32_MIN && quotient <= INT32_MAX)
            assert(twin_fixed_div(a, b) == quotient);
    }
#endif
}

#if defined(__SIZEOF_INT128__)
static void check_wide_pair(twin_xfixed_t a, twin_xfixed_t b)
{
    __int128 product = ((__int128) a * b) >> 32;
    if (product >= INT64_MIN && product <= INT64_MAX)
        assert(twin_xfixed_mul(a, b) == product);
    if (b) {
        __int128 quotient = ((__int128) a * INT64_C(4294967296)) / b;
        if (quotient >= INT64_MIN && quotient <= INT64_MAX)
            assert(twin_xfixed_div(a, b) == quotient);
    }
}

/* Independent 96-bit radicand oracle. The legacy implementation quantizes
 * larger roots before rescaling; mask those lost bits after exact sqrt.
 */
static twin_xfixed_t reference_wide_sqrt(twin_xfixed_t input)
{
    if (input <= 0)
        return 0;
    if ((uint64_t) input - (TWIN_XFIXED_ONE - 32768u) <= 65536u)
        return TWIN_XFIXED_ONE;
    __uint128_t n = (__uint128_t) (uint64_t) input << 32;
    __uint128_t root = 0, bit = (__uint128_t) 1 << 94;
    while (bit > n)
        bit >>= 2;
    while (bit) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    int highest = 63 - __builtin_clzll((uint64_t) input);
    int lost = (highest + 1) / 2 - 15;
    uint64_t mask = lost > 0 ? (UINT64_C(1) << lost) - 1 : 0;
    return (twin_xfixed_t) ((uint64_t) root & ~mask);
}
#endif

static void check_scalar_ranges(void)
{
    static const twin_fixed_t edges[] = {
        INT32_MIN, INT32_MIN + 1, -65537, -65536, -32768,        -1,       0, 1,
        32767,     32768,         65535,  65536,  INT32_MAX - 1, INT32_MAX};
    for (size_t i = 0; i < sizeof(edges) / sizeof(*edges); i++)
        for (size_t j = 0; j < sizeof(edges) / sizeof(*edges); j++)
            check_scalar_pair(edges[i], edges[j]);
    uint32_t state = 42;
    for (int i = 0; i < 20000; i++) {
        state = next_random(state);
        twin_fixed_t a = (int32_t) state;
        state = next_random(state);
        twin_fixed_t b = (int32_t) state;
        check_scalar_pair(a, b);
#if defined(__SIZEOF_INT128__)
        twin_xfixed_t wa = (twin_xfixed_t) a * 65536 + (state & 0xffff);
        twin_xfixed_t wb = (twin_xfixed_t) b * 65536 + (state & 0xffff);
        check_wide_pair(wa, wb);
        check_wide_pair(wa, (twin_xfixed_t) b * INT64_C(4294967296));
        check_wide_pair(a, wb);
#endif
    }
#if defined(__SIZEOF_INT128__)
    static const twin_xfixed_t wide_edges[] = {INT64_MIN,
                                               INT64_MIN + 1,
                                               INT32_MIN,
                                               INT32_MAX,
                                               -TWIN_XFIXED_ONE,
                                               -1,
                                               0,
                                               1,
                                               TWIN_XFIXED_ONE,
                                               INT64_MAX};
    for (size_t i = 0; i < sizeof(wide_edges) / sizeof(*wide_edges); i++)
        for (size_t j = 0; j < sizeof(wide_edges) / sizeof(*wide_edges); j++)
            check_wide_pair(wide_edges[i], wide_edges[j]);
#endif
}

static void check_roots(void)
{
    uint32_t state = 42, hash = 2166136261u;
    for (int i = 0; i < 10000; i++) {
        state = next_random(state);
        twin_fixed_t input = state & INT32_MAX;
        twin_fixed_t result = twin_fixed_sqrt(input);
#if TWIN_MATH_FLOAT
        assert(fabs(result - sqrt((double) input * 65536.0)) < 2);
#endif
        hash = (hash ^ (uint32_t) result) * 16777619u;
        state = next_random(state);
        uint64_t wide = (uint64_t) (state & INT32_MAX) << 32;
        state = next_random(state);
        wide |= state;
        uint64_t root = _twin_xfixed_sqrt((twin_xfixed_t) wide);
#if defined(__SIZEOF_INT128__)
        assert(root == (uint64_t) reference_wide_sqrt((twin_xfixed_t) wide));
#endif
        hash = (hash ^ (uint32_t) root) * 16777619u;
        hash = (hash ^ (uint32_t) (root >> 32)) * 16777619u;
    }
#if !TWIN_MATH_FLOAT
    /* Legacy roots, including their established near-one quantization. */
    assert(hash == 0x463b237cu);
    /* The near-one snap covers ONE +/- 128 inclusive, as before. */
    assert(twin_fixed_sqrt(TWIN_FIXED_ONE - 128) == TWIN_FIXED_ONE);
    assert(twin_fixed_sqrt(TWIN_FIXED_ONE + 128) == TWIN_FIXED_ONE);
    assert(twin_fixed_sqrt(TWIN_FIXED_ONE - 129) != TWIN_FIXED_ONE);
    assert(twin_fixed_sqrt(TWIN_FIXED_ONE + 129) != TWIN_FIXED_ONE);
#endif
    /* Wide roots snap within ONE +/- 32768 inclusive in every backend. */
    assert(_twin_xfixed_sqrt(TWIN_XFIXED_ONE - 32768) == TWIN_XFIXED_ONE);
    assert(_twin_xfixed_sqrt(TWIN_XFIXED_ONE + 32768) == TWIN_XFIXED_ONE);
    assert(_twin_xfixed_sqrt(TWIN_XFIXED_ONE - 32769) != TWIN_XFIXED_ONE);
    assert(_twin_xfixed_sqrt(TWIN_XFIXED_ONE + 32769) != TWIN_XFIXED_ONE);
#if defined(__SIZEOF_INT128__)
    /* Normalized squares +/- 1 catch binary64 input rounding at boundaries. */
    for (uint64_t k = UINT64_C(0x40000000); k < UINT64_C(0x80000000);
         k += 104729) {
        twin_xfixed_t square = (twin_xfixed_t) (k * k);
        for (int d = -1; d <= 1; d++)
            assert(_twin_xfixed_sqrt(square + d) ==
                   reference_wide_sqrt(square + d));
    }
#endif
    for (int bit = 0; bit < 63; bit++) {
        twin_xfixed_t value = INT64_C(1) << bit;
        for (int d = -1; d <= 1; d++) {
            twin_xfixed_t root = _twin_xfixed_sqrt(value + d);
            assert(root >= 0);
            if (bit > 0)
                assert(root >= _twin_xfixed_sqrt((value >> 1) + d));
        }
    }
}

static void check_matrix_specializations(void)
{
    twin_matrix_t original = {
        {{81920, -32768}, {16384, 98304}, {12288, -4096}}};
    for (int operation = 0; operation < 3; operation++) {
        twin_matrix_t actual = original, transform, expected;
        twin_matrix_identity(&transform);
        if (operation == 0) {
            transform.m[2][0] = 32768;
            transform.m[2][1] = -16384;
            twin_matrix_translate(&actual, 32768, -16384);
        } else if (operation == 1) {
            transform.m[0][0] = 81920;
            transform.m[1][1] = -32768;
            twin_matrix_scale(&actual, 81920, -32768);
        } else {
            twin_fixed_t s, c;
            twin_sincos(345, &s, &c);
            transform.m[0][0] = c;
            transform.m[0][1] = s;
            transform.m[1][0] = -s;
            transform.m[1][1] = c;
            twin_matrix_rotate(&actual, 345);
        }
        twin_matrix_multiply(&expected, &transform, &original);
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 2; c++)
                assert(actual.m[r][c] == expected.m[r][c]);
    }
    /* Unaffected coefficients must retain their bits even above binary32's
     * exact integer range. Sparse transforms must not multiply them by one.
     */
    original.m[2][0] = INT32_MAX;
    original.m[2][1] = INT32_MIN;
    twin_matrix_scale(&original, 81920, -32768);
    twin_matrix_rotate(&original, 345);
    assert(original.m[2][0] == INT32_MAX);
    assert(original.m[2][1] == INT32_MIN);
    original.m[2][0] = original.m[2][1] = 0;
    original.m[0][0] = INT32_MAX;
    original.m[1][0] = INT32_MIN;
    twin_matrix_translate(&original, 0, 0);
    assert(original.m[0][0] == INT32_MAX);
    assert(original.m[1][0] == INT32_MIN);
}

static void check_random_matrices(void)
{
    uint32_t state = 17;
    for (int i = 0; i < 2000; i++) {
        twin_matrix_t a, b, expected, result;
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 2; c++) {
                state = next_random(state);
                a.m[r][c] = (int32_t) (state & 0x1fffff) - 0x100000;
                state = next_random(state);
                b.m[r][c] = (int32_t) (state & 0x1fffff) - 0x100000;
            }
        for (int alias = 0; alias < 3; alias++) {
            twin_matrix_t left = a, right = alias == 2 ? a : b;
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 2; c++) {
                    twin_fixed_t value = r == 2 ? right.m[2][c] : 0;
                    for (int n = 0; n < 2; n++)
                        value += twin_fixed_mul(left.m[r][n], right.m[n][c]);
                    expected.m[r][c] = value;
                }
            if (alias == 0) {
                twin_matrix_multiply(&left, &left, &right);
                result = left;
            } else if (alias == 1) {
                twin_matrix_multiply(&right, &left, &right);
                result = right;
            } else {
                twin_matrix_multiply(&left, &left, &left);
                result = left;
            }
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 2; c++)
                    assert(result.m[r][c] == expected.m[r][c]);
        }
        twin_fixed_t x = b.m[0][0], y = b.m[0][1], ox, oy;
        twin_matrix_transform(&a, x, y, &ox, &oy);
        assert(ox == _twin_matrix_fx(&a, x, y));
        assert(oy == _twin_matrix_fy(&a, x, y));
        /* Preserve the existing sequential stores even when outputs alias m. */
        int r = (i / 2) % 3, c = i % 2;
        twin_matrix_t aliased = a, scalar = a;
        twin_fixed_t ex = _twin_matrix_fx(&scalar, x, y);
        scalar.m[r][c] = ex;
        twin_fixed_t ey = _twin_matrix_fy(&scalar, x, y);
        twin_matrix_transform(&aliased, x, y, &aliased.m[r][c], &oy);
        assert(aliased.m[r][c] == ex && oy == ey);
        twin_matrix_transform(&a, x, y, &ox, &ox);
        assert(ox == _twin_matrix_fy(&a, x, y));
    }
}

int main(void)
{
    check_scalar_ranges();
    check_roots();
    check_matrix_specializations();
    check_random_matrices();
    for (int a = -16; a <= 16; a++) {
        for (int b = -16; b <= 16; b++) {
            twin_fixed_t x = a * 4096, y = b * 8192;
            close_fixed(twin_fixed_mul(x, y), (double) a * b / 128);
            if (b)
                close_fixed(twin_fixed_div(x, y), (double) a / (2 * b));
        }
    }
    assert(twin_fixed_mul(-1, 1) == -1);
    assert(twin_fixed_div(-1, twin_int_to_fixed(2)) == 0);
    assert(twin_fixed_sqrt(0) == 0);
    assert(twin_fixed_sqrt(-TWIN_FIXED_ONE) == 0);
    close_fixed(twin_fixed_sqrt(twin_int_to_fixed(4)), 2);
    close_fixed(twin_fixed_sqrt(twin_int_to_fixed(2)), sqrt(2));
    assert(twin_acos(TWIN_FIXED_ONE) == 0);
    assert(twin_acos(-TWIN_FIXED_ONE) == TWIN_ANGLE_180);
#if TWIN_MATH_FLOAT
    assert(twin_fixed_mul(INT32_MAX, TWIN_FIXED_ONE) == INT32_MAX);
    assert(twin_fixed_mul(INT32_MIN, TWIN_FIXED_ONE) == INT32_MIN);
#endif
    twin_matrix_t a, b, result;
    twin_matrix_identity(&a);
    twin_matrix_identity(&b);
    twin_matrix_scale(&a, twin_int_to_fixed(2), twin_int_to_fixed(3));
    twin_matrix_translate(&b, twin_int_to_fixed(4), twin_int_to_fixed(-5));
    twin_matrix_multiply(&result, &a, &b);
    twin_fixed_t x, y;
    twin_matrix_transform(&result, twin_int_to_fixed(7), twin_int_to_fixed(11),
                          &x, &y);
    close_fixed(x, 18);
    close_fixed(y, 28);
    twin_matrix_multiply(&a, &a, &b); /* result aliases an input */
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 2; c++)
            assert(a.m[r][c] == result.m[r][c]);
    for (int r = 0; r < 3; r++) {
        for (int col = 0; col < 2; col++) {
            a.m[r][col] = (r * 7 - col * 3 - 5) * 8192;
            b.m[r][col] = (col * 11 - r * 2 + 1) * 4096;
        }
    }
    twin_matrix_t reference;
    for (int r = 0; r < 3; r++) {
        for (int col = 0; col < 2; col++) {
            twin_fixed_t value = r == 2 ? b.m[2][col] : 0;
            for (int n = 0; n < 2; n++)
                value += twin_fixed_mul(a.m[r][n], b.m[n][col]);
            reference.m[r][col] = value;
        }
    }
    twin_matrix_multiply(&b, &a, &b);
    for (int r = 0; r < 3; r++)
        for (int col = 0; col < 2; col++)
            assert(b.m[r][col] == reference.m[r][col]);
    uint32_t hash = 2166136261u;
    for (int angle = INT16_MIN; angle <= INT16_MAX; angle++) {
        twin_fixed_t s, c, only;
        twin_sincos(angle, &s, &c);
        double radians = angle * (6.2831853071795864769 / TWIN_ANGLE_360);
#if TWIN_MATH_FLOAT
        /* Polynomial range reduction and FMA must stay within two Q16 LSBs. */
        assert(fabs(s - sin(radians) * 65536.0) < 2);
        assert(fabs(c - cos(radians) * 65536.0) < 2);
#elif defined(CONFIG_MATH_TRIG_TABLE) && CONFIG_MATH_TRIG_TABLE
        /* Nearest Q16 sample, including mirrored negative quadrants. */
        assert(fabs(s - sin(radians) * 65536.0) <= 0.500000001);
        assert(fabs(c - cos(radians) * 65536.0) <= 0.500000001);
#else
        assert(fabs(s - sin(radians) * 65536.0) < 6);
        assert(fabs(c - cos(radians) * 65536.0) < 6);
#endif
        if (angle >= 0 && angle <= TWIN_ANGLE_90) {
            twin_fixed_t previous = 0;
            if (angle)
                twin_sincos(angle - 1, &previous, NULL);
            assert(s >= previous && s <= TWIN_FIXED_ONE);
        }
        twin_sincos(angle, &only, NULL);
        assert(only == s);
        twin_sincos(angle, NULL, &only);
        assert(only == c);
        if (angle >= 0 && angle < TWIN_ANGLE_360) {
            hash = (hash ^ (uint32_t) s) * 16777619u;
            hash = (hash ^ (uint32_t) c) * 16777619u;
        }
    }
#if !TWIN_MATH_FLOAT
#if defined(CONFIG_MATH_TRIG_TABLE) && CONFIG_MATH_TRIG_TABLE
    assert(hash == 0xd762c0fdu);
#else
    assert(hash == 0xe3ecd53du);
#endif
#endif
    for (int x = -TWIN_FIXED_ONE; x <= TWIN_FIXED_ONE; x++) {
        double expected = acos((double) x / 65536.0) *
                          (TWIN_ANGLE_360 / 6.2831853071795864769);
        assert(fabs(twin_acos(x) - expected) < 2);
    }
    twin_fixed_t s, c;
    twin_sincos(TWIN_ANGLE_90, &s, &c);
    assert(s == TWIN_FIXED_ONE && c == 0);
    twin_sincos(0, NULL, &c);
    assert(c == TWIN_FIXED_ONE);
    return 0;
}
