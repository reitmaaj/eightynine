#include <stdio.h>
#include "numeric.h"

static int failures;

static w89_f32 f32b(w89_u32 u) { return w89_bits_f32(u); }
static w89_f64 f64b(w89_u64 u) { return w89_bits_f64(u); }

static void chk_u32(const char *name, w89_u32 got, w89_u32 expect)
{
    if (got != expect) {
        fprintf(stderr, "FAIL: %s: got 0x%08lX, expected 0x%08lX\n",
                name, (unsigned long)got, (unsigned long)expect);
        failures++;
    }
}

static void chk_u64(const char *name, w89_u64 got, w89_u64 expect)
{
    if (got != expect) {
        fprintf(stderr, "FAIL: %s: got 0x%016lX, expected 0x%016lX\n",
                name, (unsigned long)got, (unsigned long)expect);
        failures++;
    }
}

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

static void t32_s_f32(const char *name, w89_f32 z, int expect_ok, w89_u32 expect)
{
    w89_u32 out = 0;
    int ok = w89_i32_trunc_f32_s(z, &out);
    if (ok != expect_ok) {
        fprintf(stderr, "FAIL: %s: trap status %d, expected %d\n", name, ok, expect_ok);
        failures++;
        return;
    }
    if (ok) {
        chk_u32(name, out, expect);
    }
}

static void t32_u_f32(const char *name, w89_f32 z, int expect_ok, w89_u32 expect)
{
    w89_u32 out = 0;
    int ok = w89_i32_trunc_f32_u(z, &out);
    if (ok != expect_ok) {
        fprintf(stderr, "FAIL: %s: trap status %d, expected %d\n", name, ok, expect_ok);
        failures++;
        return;
    }
    if (ok) {
        chk_u32(name, out, expect);
    }
}

static void t32_s_f64(const char *name, w89_f64 z, int expect_ok, w89_u32 expect)
{
    w89_u32 out = 0;
    int ok = w89_i32_trunc_f64_s(z, &out);
    if (ok != expect_ok) {
        fprintf(stderr, "FAIL: %s: trap status %d, expected %d\n", name, ok, expect_ok);
        failures++;
        return;
    }
    if (ok) {
        chk_u32(name, out, expect);
    }
}

static void t32_u_f64(const char *name, w89_f64 z, int expect_ok, w89_u32 expect)
{
    w89_u32 out = 0;
    int ok = w89_i32_trunc_f64_u(z, &out);
    if (ok != expect_ok) {
        fprintf(stderr, "FAIL: %s: trap status %d, expected %d\n", name, ok, expect_ok);
        failures++;
        return;
    }
    if (ok) {
        chk_u32(name, out, expect);
    }
}

static void t64_s_f64(const char *name, w89_f64 z, int expect_ok, w89_u64 expect)
{
    w89_u64 out = 0;
    int ok = w89_i64_trunc_f64_s(z, &out);
    if (ok != expect_ok) {
        fprintf(stderr, "FAIL: %s: trap status %d, expected %d\n", name, ok, expect_ok);
        failures++;
        return;
    }
    if (ok) {
        chk_u64(name, out, expect);
    }
}

static void t64_u_f64(const char *name, w89_f64 z, int expect_ok, w89_u64 expect)
{
    w89_u64 out = 0;
    int ok = w89_i64_trunc_f64_u(z, &out);
    if (ok != expect_ok) {
        fprintf(stderr, "FAIL: %s: trap status %d, expected %d\n", name, ok, expect_ok);
        failures++;
        return;
    }
    if (ok) {
        chk_u64(name, out, expect);
    }
}

int main(void)
{
    w89_u32 nan32 = 0x7FC00000u;
    w89_u64 nan64 = 0x7FF8000000000000UL;

    chk_u32("i32.wrap_i64", w89_i32_wrap_i64(0x0000000100000001UL), 1);
    chk_u32("i32.wrap_i64 all", w89_i32_wrap_i64(0xFFFFFFFFFFFFFFFFUL), 0xFFFFFFFFu);
    chk_u64("i64.extend_i32_s", w89_i64_extend_i32_s(0xFFFFFFFFu), 0xFFFFFFFFFFFFFFFFUL);
    chk_u64("i64.extend_i32_s 0", w89_i64_extend_i32_s(0x00000001u), 1);
    chk_u64("i64.extend_i32_u", w89_i64_extend_i32_u(0xFFFFFFFFu), 0x00000000FFFFFFFFUL);

    t32_s_f32("i32.trunc_f32_s 1.5", 1.5f, 1, 1);
    t32_s_f32("i32.trunc_f32_s -1.5", -1.5f, 1, 0xFFFFFFFFu);
    t32_s_f32("i32.trunc_f32_s min", -2147483648.0f, 1, 0x80000000u);
    t32_s_f32("i32.trunc_f32_s below min", -2147483904.0f, 0, 0);
    t32_s_f32("i32.trunc_f32_s 2^31", 2147483648.0f, 0, 0);
    t32_s_f32("i32.trunc_f32_s 2^31-128", 2147483520.0f, 1, 0x7FFFFF80u);
    t32_s_f32("i32.trunc_f32_s nan", f32b(nan32), 0, 0);
    t32_s_f32("i32.trunc_f32_s inf", f32b(0x7F800000u), 0, 0);
    t32_u_f32("i32.trunc_f32_u -0.5", -0.5f, 1, 0);
    t32_u_f32("i32.trunc_f32_u -1", -1.0f, 0, 0);
    t32_u_f32("i32.trunc_f32_u 5.9", 5.9f, 1, 5);
    t32_u_f32("i32.trunc_f32_u 2^32", 4294967296.0f, 0, 0);
    t32_u_f32("i32.trunc_f32_u 2^32-512", 4294966784.0f, 1, 0xFFFFFE00u);

    t32_s_f64("i32.trunc_f64_s boundary -0.5ulp", -2147483648.5, 1, 0x80000000u);
    t32_s_f64("i32.trunc_f64_s boundary -1", -2147483649.0, 0, 0);
    t32_s_f64("i32.trunc_f64_s max", 2147483647.0, 1, 0x7FFFFFFFu);
    t32_s_f64("i32.trunc_f64_s 2^31", 2147483648.0, 0, 0);
    t32_u_f64("i32.trunc_f64_u max", 4294967295.0, 1, 0xFFFFFFFFu);
    t32_u_f64("i32.trunc_f64_u 2^32", 4294967296.0, 0, 0);
    t32_u_f64("i32.trunc_f64_u -0.9", -0.9, 1, 0);

    t64_s_f64("i64.trunc_f64_s 2^63-1024", 9223372036854774784.0, 1, 0x7FFFFFFFFFFFFC00UL);
    t64_s_f64("i64.trunc_f64_s 2^63", 9223372036854775808.0, 0, 0);
    t64_s_f64("i64.trunc_f64_s -2^63", -9223372036854775808.0, 1, 0x8000000000000000UL);
    t64_s_f64("i64.trunc_f64_s below -2^63", -9223372036854777856.0, 0, 0);
    t64_u_f64("i64.trunc_f64_u 2^64-2048", 18446744073709549568.0, 1, 0xFFFFFFFFFFFFF800UL);
    t64_u_f64("i64.trunc_f64_u 2^64", 18446744073709551616.0, 0, 0);
    t64_u_f64("i64.trunc_f64_u 2^63", 9223372036854775808.0, 1, 0x8000000000000000UL);
    t64_u_f64("i64.trunc_f64_u -0.5", -0.5, 1, 0);
    t64_s_f64("i64.trunc_f64_s nan", f64b(nan64), 0, 0);
    t64_u_f64("i64.trunc_f64_u inf", f64b(0x7FF0000000000000UL), 0, 0);

    chk_u32("sat_f32_s nan", w89_i32_trunc_sat_f32_s(f32b(nan32)), 0);
    chk_u32("sat_f32_s +inf", w89_i32_trunc_sat_f32_s(f32b(0x7F800000u)), 0x7FFFFFFFu);
    chk_u32("sat_f32_s -inf", w89_i32_trunc_sat_f32_s(f32b(0xFF800000u)), 0x80000000u);
    chk_u32("sat_f32_s 1e10", w89_i32_trunc_sat_f32_s(1e10f), 0x7FFFFFFFu);
    chk_u32("sat_f32_s -1e10", w89_i32_trunc_sat_f32_s(-1e10f), 0x80000000u);
    chk_u32("sat_f32_s 5.9", w89_i32_trunc_sat_f32_s(5.9f), 5);
    chk_u32("sat_f32_u nan", w89_i32_trunc_sat_f32_u(f32b(nan32)), 0);
    chk_u32("sat_f32_u -1e10", w89_i32_trunc_sat_f32_u(-1e10f), 0);
    chk_u32("sat_f32_u 1e10", w89_i32_trunc_sat_f32_u(1e10f), 0xFFFFFFFFu);
    chk_u32("sat_f32_u -0.5", w89_i32_trunc_sat_f32_u(-0.5f), 0);
    chk_u64("sat_f64_s 1e19", w89_i64_trunc_sat_f64_s(1e19), 0x7FFFFFFFFFFFFFFFUL);
    chk_u64("sat_f64_s -1e19", w89_i64_trunc_sat_f64_s(-1e19), 0x8000000000000000UL);
    chk_u64("sat_f64_s 5.9", w89_i64_trunc_sat_f64_s(5.9), 5);
    chk_u64("sat_f64_u -1.0", w89_i64_trunc_sat_f64_u(-1.0), 0);
    chk_u64("sat_f64_u 1e19 in range", w89_i64_trunc_sat_f64_u(1e19), 0x8AC7230489E80000UL);
    chk_u64("sat_f64_u 1e20", w89_i64_trunc_sat_f64_u(1e20), 0xFFFFFFFFFFFFFFFFUL);
    chk_u64("sat_f64_u nan", w89_i64_trunc_sat_f64_u(f64b(nan64)), 0);
    chk_u64("sat_f32_u +inf", w89_i64_trunc_sat_f32_u(f32b(0x7F800000u)), 0xFFFFFFFFFFFFFFFFUL);
    chk_u64("sat_f64_s -inf", w89_i64_trunc_sat_f64_s(f64b(0xFFF0000000000000UL)), 0x8000000000000000UL);

    chk_f32("f32.convert_i32_s -1", w89_f32_convert_i32_s(0xFFFFFFFFu), 0xBF800000u);
    chk_f32("f32.convert_i32_s min", w89_f32_convert_i32_s(0x80000000u), 0xCF000000u);
    chk_f32("f32.convert_i32_u max", w89_f32_convert_i32_u(0xFFFFFFFFu), 0x4F800000u);
    chk_f32("f32.convert_i64_u max", w89_f32_convert_i64_u(0xFFFFFFFFFFFFFFFFUL), 0x5F800000u);
    chk_f32("f32.convert_i64_s min", w89_f32_convert_i64_s(0x8000000000000000UL), 0xDF000000u);
    chk_f64("f64.convert_i32_s -1", w89_f64_convert_i32_s(0xFFFFFFFFu), 0xBFF0000000000000UL);
    chk_f64("f64.convert_i32_u max", w89_f64_convert_i32_u(0xFFFFFFFFu), 0x41EFFFFFFFE00000UL);
    chk_f64("f64.convert_i64_u max", w89_f64_convert_i64_u(0xFFFFFFFFFFFFFFFFUL), 0x43F0000000000000UL);
    chk_f64("f64.convert_i64_s min", w89_f64_convert_i64_s(0x8000000000000000UL), 0xC3E0000000000000UL);
    chk_f64("f64.convert_i64_s -1", w89_f64_convert_i64_s(0xFFFFFFFFFFFFFFFFUL), 0xBFF0000000000000UL);

    chk_f64("f64.promote_f32 1.5", w89_f64_promote_f32(1.5f), 0x3FF8000000000000UL);
    chk_f32("f32.demote_f64 1.5", w89_f32_demote_f64(1.5), 0x3FC00000u);
    chk_f32("f32.demote_f64 nan canon", w89_f32_demote_f64(f64b(nan64)), nan32);
    chk_f32("f32.demote_f64 overflow", w89_f32_demote_f64(1e300), 0x7F800000u);

    chk_u32("i32.reinterpret_f32", w89_i32_reinterpret_f32(1.0f), 0x3F800000u);
    chk_f32("f32.reinterpret_i32", w89_f32_reinterpret_i32(0x3F800000u), 0x3F800000u);
    chk_u64("i64.reinterpret_f64", w89_i64_reinterpret_f64(1.0), 0x3FF0000000000000UL);
    chk_f64("f64.reinterpret_i64", w89_f64_reinterpret_i64(0x3FF0000000000000UL), 0x3FF0000000000000UL);
    chk_f32("f32 reinterpret roundtrip", w89_f32_reinterpret_i32(w89_i32_reinterpret_f32(-3.25f)), 0xC0500000u);

    chk_u32("i32.extend8_s 0x80", w89_i32_extend8_s(0x80u), 0xFFFFFF80u);
    chk_u32("i32.extend8_s 0x7F", w89_i32_extend8_s(0x7Fu), 0x7Fu);
    chk_u32("i32.extend16_s 0x8000", w89_i32_extend16_s(0x8000u), 0xFFFF8000u);
    chk_u32("i32.extend16_s 0x7FFF", w89_i32_extend16_s(0x7FFFu), 0x7FFFu);
    chk_u64("i64.extend8_s 0x80", w89_i64_extend8_s(0x80UL), 0xFFFFFFFFFFFFFF80UL);
    chk_u64("i64.extend16_s 0x8000", w89_i64_extend16_s(0x8000UL), 0xFFFFFFFFFFFF8000UL);
    chk_u64("i64.extend32_s 0x80000000", w89_i64_extend32_s(0x80000000UL), 0xFFFFFFFF80000000UL);
    chk_u64("i64.extend32_s 0x7FFFFFFF", w89_i64_extend32_s(0x7FFFFFFFUL), 0x000000007FFFFFFFUL);
    chk_u32("i32.extend8_s high bits", w89_i32_extend8_s(0x01234500u), 0u);
    chk_u32("i32.extend8_s high bits neg", w89_i32_extend8_s(0xFEDCBA80u), 0xFFFFFF80u);
    chk_u32("i32.extend16_s high bits", w89_i32_extend16_s(0x01230000u), 0u);
    chk_u32("i32.extend16_s high bits neg", w89_i32_extend16_s(0xFEDC8000u), 0xFFFF8000u);
    chk_u64("i64.extend32_s high bits", w89_i64_extend32_s(0x123456789ABCDEF0UL), 0xFFFFFFFF9ABCDEF0UL);
    chk_f64("f64.promote_f32 canonical nan",
            w89_f64_promote_f32(w89_bits_f32(W89_F32_CANON_NAN)),
            0x7FF8000000000000UL);

    if (failures) {
        fprintf(stderr, "%d conversion test(s) failed\n", failures);
        return 1;
    }
    printf("PASS: conversion tests\n");
    return 0;
}
