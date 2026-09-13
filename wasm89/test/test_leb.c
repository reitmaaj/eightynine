#include <stdio.h>
#include "leb.h"

#define FAILURES() (failures)
static int failures;

static void expect_ok(const char *name, int nbits,
                      const w89_byte *data, unsigned int len,
                      unsigned long expect)
{
    const w89_byte *p = data;
    const w89_byte *end = data + len;
    unsigned long v = 0;
    int ok;

    if (nbits == 8) {
        w89_u64 out;
        ok = w89_leb_u(&p, end, 8, &out);
        v = (unsigned long)out;
    } else if (nbits == 32) {
        w89_u32 out;
        ok = w89_leb_u32(&p, end, &out);
        v = out;
    } else {
        w89_u64 out;
        ok = w89_leb_u64(&p, end, &out);
        v = (unsigned long)out;
    }

    if (!ok) {
        fprintf(stderr, "FAIL: %s: expected success\n", name);
        failures++;
        return;
    }
    if (v != expect) {
        fprintf(stderr, "FAIL: %s: got %lu, expected %lu\n", name, v, expect);
        failures++;
        return;
    }
    if (p != end) {
        fprintf(stderr, "FAIL: %s: cursor did not advance to end\n", name);
        failures++;
        return;
    }
}

static void expect_ok_s(const char *name, int nbits,
                        const w89_byte *data, unsigned int len,
                        long expect)
{
    const w89_byte *p = data;
    const w89_byte *end = data + len;
    long v = 0;
    int ok;

    if (nbits == 8) {
        w89_i64 out;
        ok = w89_leb_s(&p, end, 8, &out);
        v = (long)out;
    } else if (nbits == 32) {
        w89_i32 out;
        ok = w89_leb_s32(&p, end, &out);
        v = out;
    } else {
        w89_i64 out;
        ok = w89_leb_s64(&p, end, &out);
        v = (long)out;
    }

    if (!ok) {
        fprintf(stderr, "FAIL: %s: expected success\n", name);
        failures++;
        return;
    }
    if (v != expect) {
        fprintf(stderr, "FAIL: %s: got %ld, expected %ld\n", name, v, expect);
        failures++;
        return;
    }
    if (p != end) {
        fprintf(stderr, "FAIL: %s: cursor did not advance to end\n", name);
        failures++;
        return;
    }
}

static void expect_fail(const char *name, int nbits,
                        const w89_byte *data, unsigned int len)
{
    const w89_byte *p = data;
    const w89_byte *end = data + len;
    int ok;

    if (nbits == 8) {
        w89_u64 out = 0xDEADBEEFDEADBEEFUL;
        ok = w89_leb_u(&p, end, 8, &out);
        if (out != 0xDEADBEEFDEADBEEFUL) {
            fprintf(stderr, "FAIL: %s: wrote value on failure\n", name);
            failures++;
            return;
        }
    } else if (nbits == 32) {
        w89_u32 out = 0xDEADBEEF;
        ok = w89_leb_u32(&p, end, &out);
        if (out != 0xDEADBEEF) {
            fprintf(stderr, "FAIL: %s: wrote value on failure\n", name);
            failures++;
            return;
        }
    } else {
        w89_u64 out = 0xDEADBEEFDEADBEEFUL;
        ok = w89_leb_u64(&p, end, &out);
        if (out != 0xDEADBEEFDEADBEEFUL) {
            fprintf(stderr, "FAIL: %s: wrote value on failure\n", name);
            failures++;
            return;
        }
    }

    if (ok) {
        fprintf(stderr, "FAIL: %s: expected failure\n", name);
        failures++;
        return;
    }
    if (p != data) {
        fprintf(stderr, "FAIL: %s: cursor advanced on failure\n", name);
        failures++;
        return;
    }
}

static void expect_fail_s(const char *name, int nbits,
                          const w89_byte *data, unsigned int len)
{
    const w89_byte *p = data;
    const w89_byte *end = data + len;
    int ok;

    if (nbits == 8) {
        w89_i64 out = 0x123456789ABCDEFL;
        ok = w89_leb_s(&p, end, 8, &out);
        if (out != 0x123456789ABCDEFL) {
            fprintf(stderr, "FAIL: %s: wrote value on failure\n", name);
            failures++;
            return;
        }
    } else if (nbits == 32) {
        w89_i32 out = 0x12345678;
        ok = w89_leb_s32(&p, end, &out);
        if (out != 0x12345678) {
            fprintf(stderr, "FAIL: %s: wrote value on failure\n", name);
            failures++;
            return;
        }
    } else {
        w89_i64 out = 0x123456789ABCDEFL;
        ok = w89_leb_s64(&p, end, &out);
        if (out != 0x123456789ABCDEFL) {
            fprintf(stderr, "FAIL: %s: wrote value on failure\n", name);
            failures++;
            return;
        }
    }

    if (ok) {
        fprintf(stderr, "FAIL: %s: expected failure\n", name);
        failures++;
        return;
    }
    if (p != data) {
        fprintf(stderr, "FAIL: %s: cursor advanced on failure\n", name);
        failures++;
        return;
    }
}

int main(void)
{
    static const w89_byte u32_zero[] = { 0x00 };
    static const w89_byte u32_three[] = { 0x03 };
    static const w89_byte u32_624485[] = { 0xE5, 0x8E, 0x26 };
    static const w89_byte u32_max[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x0F };
    static const w89_byte u32_overlong_trail[] = { 0x83, 0x00 };
    static const w89_byte u32_too_long[] = { 0x83, 0x80, 0x80, 0x80, 0x80, 0x00 };
    static const w89_byte u32_unused_bits[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x7F };
    static const w89_byte u32_truncated[] = { 0x80, 0x80, 0x80 };
    static const w89_byte u8_overlong_trail[] = { 0x83, 0x00 };
    static const w89_byte u8_unused_bits[] = { 0x83, 0x10 };
    static const w89_byte u8_too_long[] = { 0x83, 0x80, 0x00 };
    static const w89_byte u8_truncated[] = { 0x80 };
    static const w89_byte u64_zero_overlong[] = { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00 };
    static const w89_byte u64_max[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01 };
    static const w89_byte u64_bad_64[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02 };
    static const w89_byte u64_too_long[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

    static const w89_byte s32_minus1[] = { 0x7F };
    static const w89_byte s32_zero[] = { 0x00 };
    static const w89_byte s32_minus2[] = { 0x7E };
    static const w89_byte s32_min[] = { 0x80, 0x80, 0x80, 0x80, 0x78 };
    static const w89_byte s32_max[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x07 };
    static const w89_byte s32_overlong_minus2[] = { 0xFE, 0xFF, 0x7F };
    static const w89_byte s32_bad_sign[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x0F };
    static const w89_byte s32_truncated[] = { 0x80, 0x80 };
    static const w89_byte s8_minus1[] = { 0x7F };
    static const w89_byte s8_minus2[] = { 0x7E };
    static const w89_byte s8_overlong_minus1[] = { 0xFF, 0x7F };
    static const w89_byte s8_bad_1[] = { 0x83, 0x3E };
    static const w89_byte s8_bad_2[] = { 0xFF, 0x7B };
    static const w89_byte s8_too_long[] = { 0xFE, 0xFF, 0x7F };
    static const w89_byte s8_truncated[] = { 0x80, 0x80 };
    static const w89_byte s64_min[] = { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7F };
    static const w89_byte s64_max[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
    static const w89_byte s64_bad_consistency[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x40 };
    static const w89_byte s64_too_long[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
    static const w89_byte s64_truncated[] = { 0x80, 0x80, 0x80 };

    expect_ok("u32 0", 32, u32_zero, 1, 0);
    expect_ok("u32 3", 32, u32_three, 1, 3);
    expect_ok("u32 624485", 32, u32_624485, 3, 624485);
    expect_ok("u32 max", 32, u32_max, 5, 0xFFFFFFFFUL);
    expect_ok("u32 overlong trailing zeros", 32, u32_overlong_trail, 2, 3);
    expect_fail("u32 too long", 32, u32_too_long, 6);
    expect_fail("u32 unused bits", 32, u32_unused_bits, 5);
    expect_fail("u32 truncated", 32, u32_truncated, 3);
    expect_fail("u32 empty", 32, u32_zero, 0);

    expect_ok("u8 overlong trailing zeros", 8, u8_overlong_trail, 2, 3);
    expect_fail("u8 unused bits", 8, u8_unused_bits, 2);
    expect_fail("u8 too long", 8, u8_too_long, 3);
    expect_fail("u8 truncated", 8, u8_truncated, 1);

    expect_ok("u64 zero overlong", 64, u64_zero_overlong, 10, 0);
    expect_ok("u64 max", 64, u64_max, 10, 0xFFFFFFFFFFFFFFFFUL);
    expect_fail("u64 bit 64 set", 64, u64_bad_64, 10);
    expect_fail("u64 too long", 64, u64_too_long, 11);

    expect_ok_s("s32 -1", 32, s32_minus1, 1, -1);
    expect_ok_s("s32 0", 32, s32_zero, 1, 0);
    expect_ok_s("s32 -2", 32, s32_minus2, 1, -2);
    expect_ok_s("s32 min", 32, s32_min, 5, -2147483648L);
    expect_ok_s("s32 max", 32, s32_max, 5, 2147483647L);
    expect_ok_s("s32 overlong -2", 32, s32_overlong_minus2, 3, -2);
    expect_fail_s("s32 bad sign extension", 32, s32_bad_sign, 5);
    expect_fail_s("s32 truncated", 32, s32_truncated, 2);
    expect_ok_s("s8 -1", 8, s8_minus1, 1, -1);
    expect_ok_s("s8 -2", 8, s8_minus2, 1, -2);
    expect_ok_s("s8 overlong -1", 8, s8_overlong_minus1, 2, -1);
    expect_fail_s("s8 bad 0x83 0x3E", 8, s8_bad_1, 2);
    expect_fail_s("s8 bad 0xFF 0x7B", 8, s8_bad_2, 2);
    expect_fail_s("s8 too long", 8, s8_too_long, 3);
    expect_fail_s("s8 truncated", 8, s8_truncated, 2);

    expect_ok_s("s64 min", 64, s64_min, 10, -9223372036854775807L - 1);
    expect_ok_s("s64 max", 64, s64_max, 10, 9223372036854775807L);
    expect_fail_s("s64 bad consistency", 64, s64_bad_consistency, 10);
    expect_fail_s("s64 too long", 64, s64_too_long, 11);
    expect_fail_s("s64 truncated", 64, s64_truncated, 3);

    if (failures) {
        fprintf(stderr, "%d LEB128 test(s) failed\n", failures);
        return 1;
    }
    printf("PASS: LEB128 tests\n");
    return 0;
}
