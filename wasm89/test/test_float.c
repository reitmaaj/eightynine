#include <stdio.h>
#include "numeric.h"

static int failures;

static w89_f32 f32b(w89_u32 u) { return w89_bits_f32(u); }
static w89_f64 f64b(w89_u64 u) { return w89_bits_f64(u); }

static void chk_f32(const char *name, w89_f32 got, w89_u32 expect)
{
    w89_u32 g = w89_f32_bits(got);
    if (g != expect) {
        fprintf(stderr, "FAIL: %s: got 0x%08lX, expected 0x%08lX\n",
                name, (unsigned long)g, (unsigned long)expect);
        failures++;
    }
}

static void chk_f64(const char *name, w89_f64 got, w89_u64 expect)
{
    w89_u64 g = w89_f64_bits(got);
    if (g != expect) {
        fprintf(stderr, "FAIL: %s: got 0x%016lX, expected 0x%016lX\n",
                name, (unsigned long)g, (unsigned long)expect);
        failures++;
    }
}

static void chk_u32(const char *name, w89_u32 got, w89_u32 expect)
{
    if (got != expect) {
        fprintf(stderr, "FAIL: %s: got %lu, expected %lu\n",
                name, (unsigned long)got, (unsigned long)expect);
        failures++;
    }
}

int main(void)
{
    const w89_u32 nan32 = 0x7FC00000u;
    const w89_u64 nan64 = 0x7FF8000000000000UL;
    w89_u32 payload32 = 0x7FFFFFFFu;
    w89_u64 payload64 = 0x7FFFFFFFFFFFFFFFUL;
    w89_f32 pinf32 = f32b(0x7F800000u);
    w89_f32 ninf32 = f32b(0xFF800000u);
    w89_f64 pinf64 = f64b(0x7FF0000000000000UL);
    w89_f64 ninf64 = f64b(0xFFF0000000000000UL);

    chk_f32("f32 add 1.5+2.25", w89_f32_add(1.5f, 2.25f), 0x40700000u);
    chk_f32("f32 add overflow", w89_f32_add(3.4028235e38f, 3.4028235e38f), 0x7F800000u);
    chk_f32("f32 sub 1.0-1.0", w89_f32_sub(1.0f, 1.0f), 0x00000000u);
    chk_f32("f32 sub 0.0-1.0", w89_f32_sub(0.0f, 1.0f), 0xBF800000u);
    chk_f32("f32 mul 1.5*2.0", w89_f32_mul(1.5f, 2.0f), 0x40400000u);
    chk_f32("f32 div 1/3", w89_f32_div(1.0f, 3.0f), 0x3EAAAAABu);
    chk_f32("f32 div 0.1", w89_f32_div(1.0f, 10.0f), 0x3DCCCCCDu);
    chk_f32("f32 div 1/0", w89_f32_div(1.0f, 0.0f), 0x7F800000u);
    chk_f32("f32 div -1/0", w89_f32_div(-1.0f, 0.0f), 0xFF800000u);
    chk_f32("f32 div 0/0 canon", w89_f32_div(0.0f, 0.0f), nan32);
    chk_f32("f32 inf-inf canon", w89_f32_sub(pinf32, pinf32), 0x7FC00000u);
    chk_f32("f32 mul 0*inf canon", w89_f32_mul(0.0f, pinf32), nan32);

    chk_f32("f32 min 1,2", w89_f32_min(1.0f, 2.0f), 0x3F800000u);
    chk_f32("f32 min 2,1", w89_f32_min(2.0f, 1.0f), 0x3F800000u);
    chk_f32("f32 min nan", w89_f32_min(f32b(nan32), 1.0f), nan32);
    chk_f32("f32 min -inf", w89_f32_min(ninf32, 5.0f), 0xFF800000u);
    chk_f32("f32 min +inf", w89_f32_min(5.0f, pinf32), 0x40A00000u);
    chk_f32("f32 min -0,+0", w89_f32_min(-0.0f, 0.0f), 0x80000000u);
    chk_f32("f32 min +0,-0", w89_f32_min(0.0f, -0.0f), 0x80000000u);
    chk_f32("f32 min -0,-0", w89_f32_min(-0.0f, -0.0f), 0x80000000u);
    chk_f32("f32 min +0,+0", w89_f32_min(0.0f, 0.0f), 0x00000000u);
    chk_f32("f32 max 1,2", w89_f32_max(1.0f, 2.0f), 0x40000000u);
    chk_f32("f32 max nan", w89_f32_max(1.0f, f32b(nan32)), nan32);
    chk_f32("f32 max +inf", w89_f32_max(5.0f, pinf32), 0x7F800000u);
    chk_f32("f32 max -0,+0", w89_f32_max(-0.0f, 0.0f), 0x00000000u);
    chk_f32("f32 max +0,-0", w89_f32_max(0.0f, -0.0f), 0x00000000u);

    chk_f32("f32 copysign", w89_f32_copysign(1.5f, -2.0f), 0xBFC00000u);
    chk_f32("f32 abs", w89_f32_abs(-1.5f), 0x3FC00000u);
    chk_f32("f32 neg", w89_f32_neg(1.5f), 0xBFC00000u);
    chk_f32("f32 neg -1.5", w89_f32_neg(-1.5f), 0x3FC00000u);
    chk_f32("f32 abs preserves nan payload", w89_f32_abs(f32b(payload32)), payload32);
    chk_f32("f32 neg preserves nan payload", w89_f32_neg(f32b(payload32)), 0xFFFFFFFFu);

    chk_f32("f32 sqrt 2", w89_f32_sqrt(2.0f), 0x3FB504F3u);
    chk_f32("f32 sqrt 4", w89_f32_sqrt(4.0f), 0x40000000u);
    chk_f32("f32 sqrt -1 canon", w89_f32_sqrt(-1.0f), nan32);
    chk_f32("f32 ceil 1.1", w89_f32_ceil(1.1f), 0x40000000u);
    chk_f32("f32 ceil -1.1", w89_f32_ceil(-1.1f), 0xBF800000u);
    chk_f32("f32 floor 1.9", w89_f32_floor(1.9f), 0x3F800000u);
    chk_f32("f32 floor -1.1", w89_f32_floor(-1.1f), 0xC0000000u);
    chk_f32("f32 trunc 1.9", w89_f32_trunc(1.9f), 0x3F800000u);
    chk_f32("f32 trunc -1.9", w89_f32_trunc(-1.9f), 0xBF800000u);
    chk_f32("f32 trunc -0.5 signed zero", w89_f32_trunc(-0.5f), 0x80000000u);
    chk_f32("f32 nearest 1.5", w89_f32_nearest(1.5f), 0x40000000u);
    chk_f32("f32 nearest 2.5", w89_f32_nearest(2.5f), 0x40000000u);
    chk_f32("f32 nearest -0.5", w89_f32_nearest(-0.5f), 0x80000000u);
    chk_f32("f32 nearest -2.5", w89_f32_nearest(-2.5f), 0xC0000000u);
    chk_f32("f32 nearest 3.5", w89_f32_nearest(3.5f), 0x40800000u);
    chk_f32("f32 nearest 0.4", w89_f32_nearest(0.4f), 0x00000000u);
    chk_f32("f32 nearest -0.4", w89_f32_nearest(-0.4f), 0x80000000u);
    chk_f32("f32 nearest 2.4", w89_f32_nearest(2.4f), 0x40000000u);
    chk_f32("f32 nearest 2.6", w89_f32_nearest(2.6f), 0x40400000u);
    chk_f32("f32 nearest inf", w89_f32_nearest(pinf32), 0x7F800000u);
    chk_f32("f32 nearest nan", w89_f32_nearest(f32b(0x7FA12345u)), nan32);
    chk_f32("f32 floor inf", w89_f32_floor(pinf32), 0x7F800000u);
    chk_f32("f32 ceil -inf", w89_f32_ceil(ninf32), 0xFF800000u);

    chk_u32("f32 eq 1,1", w89_f32_eq(1.0f, 1.0f), 1);
    chk_u32("f32 eq +0,-0", w89_f32_eq(0.0f, -0.0f), 1);
    chk_u32("f32 eq nan", w89_f32_eq(f32b(nan32), f32b(nan32)), 0);
    chk_u32("f32 ne nan", w89_f32_ne(f32b(nan32), f32b(nan32)), 1);
    chk_u32("f32 lt 1,2", w89_f32_lt(1.0f, 2.0f), 1);
    chk_u32("f32 lt nan", w89_f32_lt(f32b(nan32), 1.0f), 0);
    chk_u32("f32 gt 2,1", w89_f32_gt(2.0f, 1.0f), 1);
    chk_u32("f32 le 1,1", w89_f32_le(1.0f, 1.0f), 1);
    chk_u32("f32 ge 1,1", w89_f32_ge(1.0f, 1.0f), 1);
    chk_u32("f32 ge nan", w89_f32_ge(1.0f, f32b(nan32)), 0);

    chk_f64("f64 add 1.5+2.25", w89_f64_add(1.5, 2.25), 0x400E000000000000UL);
    chk_f64("f64 div 1/3", w89_f64_div(1.0, 3.0), 0x3FD5555555555555UL);
    chk_f64("f64 div 0.1", w89_f64_div(1.0, 10.0), 0x3FB999999999999AUL);
    chk_f64("f64 div 1/0", w89_f64_div(1.0, 0.0), 0x7FF0000000000000UL);
    chk_f64("f64 add overflow", w89_f64_add(1.7976931348623157e308, 1.7976931348623157e308), 0x7FF0000000000000UL);
    chk_f64("f64 0/0 canon", w89_f64_div(0.0, 0.0), nan64);
    chk_f64("f64 sqrt 2", w89_f64_sqrt(2.0), 0x3FF6A09E667F3BCDUL);
    chk_f64("f64 sqrt -1 canon", w89_f64_sqrt(-1.0), nan64);
    chk_f64("f64 min -0,+0", w89_f64_min(-0.0, 0.0), 0x8000000000000000UL);
    chk_f64("f64 max -0,+0", w89_f64_max(-0.0, 0.0), 0x0000000000000000UL);
    chk_f64("f64 min -inf", w89_f64_min(ninf64, 5.0), 0xFFF0000000000000UL);
    chk_f64("f64 max +inf", w89_f64_max(5.0, pinf64), 0x7FF0000000000000UL);
    chk_f64("f64 min nan", w89_f64_min(f64b(nan64), 1.0), nan64);
    chk_f64("f64 inf-inf canon", w89_f64_sub(pinf64, pinf64), nan64);
    chk_f64("f64 nearest 2.5", w89_f64_nearest(2.5), 0x4000000000000000UL);
    chk_f64("f64 nearest -2.5", w89_f64_nearest(-2.5), 0xC000000000000000UL);
    chk_f64("f64 nearest -0.5", w89_f64_nearest(-0.5), 0x8000000000000000UL);
    chk_f64("f64 trunc -1.9", w89_f64_trunc(-1.9), 0xBFF0000000000000UL);
    chk_f64("f64 copysign", w89_f64_copysign(1.5, -2.0), 0xBFF8000000000000UL);
    chk_f64("f64 abs preserves nan payload", w89_f64_abs(f64b(payload64)), payload64);
    chk_f64("f64 neg preserves nan payload", w89_f64_neg(f64b(payload64)), 0xFFFFFFFFFFFFFFFFUL);
    chk_u32("f64 lt 1,2", w89_f64_lt(1.0, 2.0), 1);
    chk_u32("f64 eq +0,-0", w89_f64_eq(0.0, -0.0), 1);
    chk_u32("f64 ne nan", w89_f64_ne(f64b(nan64), f64b(nan64)), 1);

    if (failures) {
        fprintf(stderr, "%d float test(s) failed\n", failures);
        return 1;
    }
    printf("PASS: float numeric tests\n");
    return 0;
}
