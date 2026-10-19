/* Compile-time arithmetic policy. Coordinate storage remains Q16.16. */
#ifndef TWIN_MATH_H
#define TWIN_MATH_H

/* Included by twin.h after twin_fixed_t and TWIN_FIXED_ONE are defined.
 * Float is an approximate backend: binary32 has 24 significant bits.
 * Inputs and results must remain within the Q16.16 representable range.
 */
/* Backend and target capabilities, tested by every math source. */
#if defined(CONFIG_MATH_FLOAT) && CONFIG_MATH_FLOAT
#define TWIN_MATH_FLOAT 1
#else
#define TWIN_MATH_FLOAT 0
#endif
#if TWIN_MATH_FLOAT && defined(__riscv) && defined(__riscv_flen) && \
    __riscv_flen >= 32
#define TWIN_MATH_RV_F 1
#else
#define TWIN_MATH_RV_F 0
#endif
#if TWIN_MATH_FLOAT && defined(__aarch64__) && defined(__ARM_FP)
#define TWIN_MATH_A64_FP 1
#else
#define TWIN_MATH_A64_FP 0
#endif
#if TWIN_MATH_A64_FP && defined(__ARM_NEON)
#define TWIN_MATH_A64_NEON 1
#else
#define TWIN_MATH_A64_NEON 0
#endif

#if TWIN_MATH_FLOAT
#if TWIN_MATH_RV_F
/* Caller has already established a nonnegative, finite input. Bypass the
 * libm errno fallback that compilers retain even after integer range checks.
 */
static inline float twin_math_sqrt_positive(float value)
{
    float result;
    __asm__("fsqrt.s %0, %1" : "=f"(result) : "f"(value));
    return result;
}
#elif TWIN_MATH_A64_FP
static inline float twin_math_sqrt_positive(float value)
{
    float result;
    __asm__("fsqrt %s0, %s1" : "=w"(result) : "w"(value));
    return result;
}
static inline double twin_math_sqrt_positive64(double value)
{
    double result;
    __asm__("fsqrt %d0, %d1" : "=w"(result) : "w"(value));
    return result;
}
#else
static inline float twin_math_sqrt_positive(float value)
{
    return __builtin_sqrtf(value);
}
#endif

static inline twin_fixed_t twin_math_from_scaled(float value)
{
#if TWIN_MATH_RV_F
    /* F supplies both the rounding mode and signed saturation in one op.
     * Explicit RTZ does not depend on the process's dynamic rounding mode.
     */
    twin_fixed_t result;
    __asm__("fcvt.w.s %0, %1, rtz" : "=r"(result) : "f"(value));
    return result;
#elif TWIN_MATH_A64_FP
    twin_fixed_t result;
    __asm__("fcvtzs %s0, %s1" : "=w"(result) : "w"(value));
    return result;
#else
    /* INT32_MAX rounds to 2^31 in binary32; avoid an invalid C cast. */
    if (value >= 0x1p31f)
        return INT32_MAX;
    if (value <= -0x1p31f)
        return INT32_MIN;
    return (twin_fixed_t) value;
#endif
}
static inline twin_fixed_t twin_fixed_mul(twin_fixed_t a, twin_fixed_t b)
{
    /* Match the fixed backend's downward rounding for negative products. */
    float value = (float) a * ((float) b * 0x1p-16f);
#if TWIN_MATH_RV_F
    twin_fixed_t result;
    __asm__("fcvt.w.s %0, %1, rdn" : "=r"(result) : "f"(value));
    return result;
#elif TWIN_MATH_A64_FP
    twin_fixed_t result;
    __asm__("fcvtms %s0, %s1" : "=w"(result) : "w"(value));
    return result;
#else
    twin_fixed_t result = twin_math_from_scaled(value);
    if ((float) result > value && result != INT32_MIN)
        --result;
    return result;
#endif
}
static inline twin_fixed_t twin_fixed_div(twin_fixed_t a, twin_fixed_t b)
{
#if TWIN_MATH_A64_FP
    /* Fixed-point FCVTZS folds the exact power-of-two scale into conversion. */
    float ratio = (float) a / (float) b;
    twin_fixed_t result;
    __asm__("fcvtzs %s0, %s1, #16" : "=w"(result) : "w"(ratio));
    return result;
#else
    return twin_math_from_scaled(((float) a / (float) b) * 0x1p16f);
#endif
}
#else
#define twin_fixed_mul(a, b) ((twin_fixed_t) (((int64_t) (a) * (b)) >> 16))
/* a / b in 16.16.  When |a| * 65536 fits in 31 bits a 32-bit divide gives
 * the same quotient (no INT32_MIN / -1 either), which spares 32-bit targets
 * a 64-bit division */
static inline twin_fixed_t twin_fixed_div(twin_fixed_t a, twin_fixed_t b)
{
    /* Two 32-bit cases give the 64-bit quotient exactly: a small dividend
     * shifts without overflow, and a whole-number divisor cancels the
     * shift (except -1, whose quotient of INT32_MIN overflows) */
    if ((uint32_t) a + 0x7fffu < 0xffffu)
        return (twin_fixed_t) ((int32_t) ((uint32_t) a << 16) / b);
    if (!(b & 0xffff) && b != -TWIN_FIXED_ONE)
        return a / (b >> 16);
    return (twin_fixed_t) (((int64_t) a * 65536) / b);
}
#endif
/* Every unit including twin.h references the symbol for its backend, and the
 * library defines only its own. An application built without the library's
 * config.h then fails to link instead of silently mixing arithmetic.
 */
#if TWIN_MATH_FLOAT
#define TWIN_MATH_ABI twin_math_abi_float
#else
#define TWIN_MATH_ABI twin_math_abi_fixed
#endif
extern const char TWIN_MATH_ABI;
/* Tools that share twin.h types but do no arithmetic and do not link the
 * library (such as headless-ctl) define TWIN_MATH_NO_ABI_CHECK.
 */
#ifndef TWIN_MATH_NO_ABI_CHECK
__attribute__((used)) static const char *const twin_math_abi_ref =
    &TWIN_MATH_ABI;
#endif
#endif
