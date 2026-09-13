#include <stdio.h>
#include "numeric.h"

static int failures;

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

static void chk_trap32(const char *name, int ok, int expect_ok)
{
    if (ok != expect_ok) {
        fprintf(stderr, "FAIL: %s: trap status %d, expected %d\n",
                name, ok, expect_ok);
        failures++;
    }
}

static void div_u32(const char *name, w89_u32 a, w89_u32 b,
                    int expect_ok, w89_u32 expect)
{
    w89_u32 out = 0;
    int ok = w89_i32_div_u(a, b, &out);
    chk_trap32(name, ok, expect_ok);
    if (ok) {
        chk_u32(name, out, expect);
    }
}

static void div_s32(const char *name, w89_u32 a, w89_u32 b,
                    int expect_ok, w89_u32 expect)
{
    w89_u32 out = 0;
    int ok = w89_i32_div_s(a, b, &out);
    chk_trap32(name, ok, expect_ok);
    if (ok) {
        chk_u32(name, out, expect);
    }
}

static void rem_u32(const char *name, w89_u32 a, w89_u32 b,
                    int expect_ok, w89_u32 expect)
{
    w89_u32 out = 0;
    int ok = w89_i32_rem_u(a, b, &out);
    chk_trap32(name, ok, expect_ok);
    if (ok) {
        chk_u32(name, out, expect);
    }
}

static void rem_s32(const char *name, w89_u32 a, w89_u32 b,
                    int expect_ok, w89_u32 expect)
{
    w89_u32 out = 0;
    int ok = w89_i32_rem_s(a, b, &out);
    chk_trap32(name, ok, expect_ok);
    if (ok) {
        chk_u32(name, out, expect);
    }
}

static void div_u64(const char *name, w89_u64 a, w89_u64 b,
                    int expect_ok, w89_u64 expect)
{
    w89_u64 out = 0;
    int ok = w89_i64_div_u(a, b, &out);
    chk_trap32(name, ok, expect_ok);
    if (ok) {
        chk_u64(name, out, expect);
    }
}

static void div_s64(const char *name, w89_u64 a, w89_u64 b,
                    int expect_ok, w89_u64 expect)
{
    w89_u64 out = 0;
    int ok = w89_i64_div_s(a, b, &out);
    chk_trap32(name, ok, expect_ok);
    if (ok) {
        chk_u64(name, out, expect);
    }
}

static void rem_s64(const char *name, w89_u64 a, w89_u64 b,
                    int expect_ok, w89_u64 expect)
{
    w89_u64 out = 0;
    int ok = w89_i64_rem_s(a, b, &out);
    chk_trap32(name, ok, expect_ok);
    if (ok) {
        chk_u64(name, out, expect);
    }
}

int main(void)
{
    chk_u32("i32 add wrap", w89_i32_add(0xFFFFFFFFu, 1), 0);
    chk_u32("i32 add plain", w89_i32_add(1, 2), 3);
    chk_u32("i32 sub underflow", w89_i32_sub(0, 1), 0xFFFFFFFFu);
    chk_u32("i32 sub plain", w89_i32_sub(5, 3), 2);
    chk_u32("i32 mul wrap", w89_i32_mul(0x10000u, 0x10000u), 0);
    chk_u32("i32 mul plain", w89_i32_mul(7, 6), 42);

    div_u32("i32 div_u 7/2", 7, 2, 1, 3);
    div_u32("i32 div_u by zero", 7, 0, 0, 0);
    div_u32("i32 div_u 0xFFFFFFFF/1", 0xFFFFFFFFu, 1, 1, 0xFFFFFFFFu);
    div_s32("i32 div_s -7/2", 0xFFFFFFF9u, 2, 1, 0xFFFFFFFDu);
    div_s32("i32 div_s min/-1", 0x80000000u, 0xFFFFFFFFu, 0, 0);
    div_s32("i32 div_s min/1", 0x80000000u, 1, 1, 0x80000000u);
    div_s32("i32 div_s by zero", 1, 0, 0, 0);
    div_s32("i32 div_s 7/-2", 7, 0xFFFFFFFEu, 1, 0xFFFFFFFDu);
    rem_u32("i32 rem_u 7%2", 7, 2, 1, 1);
    rem_u32("i32 rem_u by zero", 7, 0, 0, 0);
    rem_s32("i32 rem_s -7%2", 0xFFFFFFF9u, 2, 1, 0xFFFFFFFFu);
    rem_s32("i32 rem_s 7%-2", 7, 0xFFFFFFFEu, 1, 1);
    rem_s32("i32 rem_s min%-1", 0x80000000u, 0xFFFFFFFFu, 1, 0);

    chk_u32("i32 and", w89_i32_and(0xF0F0F0F0u, 0x0F0F0F0Fu), 0);
    chk_u32("i32 or", w89_i32_or(0xF0F0F0F0u, 0x0F0F0F0Fu), 0xFFFFFFFFu);
    chk_u32("i32 xor", w89_i32_xor(0xFFFFFFFFu, 0x00FF00FFu), 0xFF00FF00u);
    chk_u32("i32 shl 1<<1", w89_i32_shl(1, 1), 2);
    chk_u32("i32 shl count mod 32", w89_i32_shl(1, 32), 1);
    chk_u32("i32 shl count mod 33", w89_i32_shl(1, 33), 2);
    chk_u32("i32 shl 0x80000000<<1", w89_i32_shl(0x80000000u, 1), 0);
    chk_u32("i32 shr_u 0x80000000>>31", w89_i32_shr_u(0x80000000u, 31), 1);
    chk_u32("i32 shr_u count mod", w89_i32_shr_u(0x80000000u, 63), 1);
    chk_u32("i32 shr_s 0x80000000>>31", w89_i32_shr_s(0x80000000u, 31), 0xFFFFFFFFu);
    chk_u32("i32 shr_s 0x40000000>>31", w89_i32_shr_s(0x40000000u, 31), 0);
    chk_u32("i32 shr_s by 0", w89_i32_shr_s(0x80000000u, 0), 0x80000000u);
    chk_u32("i32 rotl 0x80000000,1", w89_i32_rotl(0x80000000u, 1), 1);
    chk_u32("i32 rotl by 0", w89_i32_rotl(0x80000000u, 0), 0x80000000u);
    chk_u32("i32 rotl by 32", w89_i32_rotl(0x80000000u, 32), 0x80000000u);
    chk_u32("i32 rotr 1,1", w89_i32_rotr(1, 1), 0x80000000u);
    chk_u32("i32 rotr 0x80000000,31", w89_i32_rotr(0x80000000u, 31), 1);

    chk_u32("i32 clz 0", w89_i32_clz(0), 32);
    chk_u32("i32 clz 0x80000000", w89_i32_clz(0x80000000u), 0);
    chk_u32("i32 clz 1", w89_i32_clz(1), 31);
    chk_u32("i32 ctz 0", w89_i32_ctz(0), 32);
    chk_u32("i32 ctz 1", w89_i32_ctz(1), 0);
    chk_u32("i32 ctz 0x80000000", w89_i32_ctz(0x80000000u), 31);
    chk_u32("i32 ctz 0x80000000|1", w89_i32_ctz(0x80000001u), 0);
    chk_u32("i32 popcnt 0", w89_i32_popcnt(0), 0);
    chk_u32("i32 popcnt all", w89_i32_popcnt(0xFFFFFFFFu), 32);
    chk_u32("i32 popcnt 0x0F0F0F0F", w89_i32_popcnt(0x0F0F0F0Fu), 16);
    chk_u32("i32 eqz 0", w89_i32_eqz(0), 1);
    chk_u32("i32 eqz 1", w89_i32_eqz(1), 0);

    chk_u32("i32 eq", w89_i32_eq(1, 1), 1);
    chk_u32("i32 ne", w89_i32_ne(1, 2), 1);
    chk_u32("i32 lt_u 0xFFFFFFFF<1", w89_i32_lt_u(0xFFFFFFFFu, 1), 0);
    chk_u32("i32 lt_s -1<1", w89_i32_lt_s(0xFFFFFFFFu, 1), 1);
    chk_u32("i32 lt_s 1<-1", w89_i32_lt_s(1, 0xFFFFFFFFu), 0);
    chk_u32("i32 gt_s 1>-1", w89_i32_gt_s(1, 0xFFFFFFFFu), 1);
    chk_u32("i32 ge_s -1>=-1", w89_i32_ge_s(0xFFFFFFFFu, 0xFFFFFFFFu), 1);
    chk_u32("i32 le_s -1<=1", w89_i32_le_s(0xFFFFFFFFu, 1), 1);
    chk_u32("i32 gt_u 0xFFFFFFFF>1", w89_i32_gt_u(0xFFFFFFFFu, 1), 1);
    chk_u32("i32 le_u 1<=1", w89_i32_le_u(1, 1), 1);
    chk_u32("i32 ge_u 0>=0", w89_i32_ge_u(0, 0), 1);

    chk_u64("i64 add wrap", w89_i64_add(0xFFFFFFFFFFFFFFFFUL, 1), 0);
    chk_u64("i64 sub underflow", w89_i64_sub(0, 1), 0xFFFFFFFFFFFFFFFFUL);
    chk_u64("i64 mul wrap", w89_i64_mul(0x100000000UL, 0x100000000UL), 0);
    div_u64("i64 div_u 7/2", 7, 2, 1, 3);
    div_u64("i64 div_u by zero", 7, 0, 0, 0);
    div_s64("i64 div_s -7/2", 0xFFFFFFFFFFFFFFF9UL, 2, 1, 0xFFFFFFFFFFFFFFFDUL);
    div_s64("i64 div_s min/-1", 0x8000000000000000UL, 0xFFFFFFFFFFFFFFFFUL, 0, 0);
    div_s64("i64 div_s min/1", 0x8000000000000000UL, 1, 1, 0x8000000000000000UL);
    div_s64("i64 div_s by zero", 1, 0, 0, 0);
    rem_s64("i64 rem_s min%-1", 0x8000000000000000UL, 0xFFFFFFFFFFFFFFFFUL, 1, 0);
    rem_s64("i64 rem_s -7%2", 0xFFFFFFFFFFFFFFF9UL, 2, 1, 0xFFFFFFFFFFFFFFFFUL);
    chk_u64("i64 shl count mod", w89_i64_shl(1, 64), 1);
    chk_u64("i64 shl count mod 65", w89_i64_shl(1, 65), 2);
    chk_u64("i64 shr_s 0x8000..>>63", w89_i64_shr_s(0x8000000000000000UL, 63), 0xFFFFFFFFFFFFFFFFUL);
    chk_u64("i64 shr_s by 0", w89_i64_shr_s(0x8000000000000000UL, 0), 0x8000000000000000UL);
    chk_u64("i64 rotl 0x8000..,1", w89_i64_rotl(0x8000000000000000UL, 1), 1);
    chk_u64("i64 rotr 1,1", w89_i64_rotr(1, 1), 0x8000000000000000UL);
    chk_u32("i64 clz 0", w89_i64_clz(0), 64);
    chk_u32("i64 clz 1", w89_i64_clz(1), 63);
    chk_u32("i64 ctz 0", w89_i64_ctz(0), 64);
    chk_u32("i64 ctz 0x8000..", w89_i64_ctz(0x8000000000000000UL), 63);
    chk_u32("i64 popcnt all", w89_i64_popcnt(0xFFFFFFFFFFFFFFFFUL), 64);
    chk_u32("i64 popcnt 0", w89_i64_popcnt(0), 0);
    chk_u32("i64 lt_s -1<1", w89_i64_lt_s(0xFFFFFFFFFFFFFFFFUL, 1), 1);
    chk_u32("i64 lt_u max<1", w89_i64_lt_u(0xFFFFFFFFFFFFFFFFUL, 1), 0);
    chk_u32("i64 gt_u max>1", w89_i64_gt_u(0xFFFFFFFFFFFFFFFFUL, 1), 1);
    chk_u32("i64 eq", w89_i64_eq(5, 5), 1);
    chk_u32("i64 ne", w89_i64_ne(5, 6), 1);

    if (failures) {
        fprintf(stderr, "%d numeric test(s) failed\n", failures);
        return 1;
    }
    printf("PASS: integer numeric tests\n");
    return 0;
}
