/* test_main.c - runner and check harness for the libcksum89 suite. */

#include <stdio.h>

#include "test.h"

int cksum89_test_failures = 0;
int cksum89_test_checks = 0;

void cksum89_test_check(int cond, const char *what)
{
    ++cksum89_test_checks;
    if (cond == 0)
    {
        ++cksum89_test_failures;
        fprintf(stderr, "FAIL: %s\n", what);
    }
}

void cksum89_test_u16(cksum89_u16 got, cksum89_u16 want, const char *what)
{
    ++cksum89_test_checks;
    if (got != want)
    {
        ++cksum89_test_failures;
        fprintf(stderr, "FAIL: %s: got 0x%04x want 0x%04x\n", what,
                (unsigned int)got, (unsigned int)want);
    }
}

void cksum89_test_u32(cksum89_u32 got, cksum89_u32 want, const char *what)
{
    ++cksum89_test_checks;
    if (got != want)
    {
        ++cksum89_test_failures;
        fprintf(stderr, "FAIL: %s: got 0x%08lx want 0x%08lx\n", what,
                (unsigned long)got, (unsigned long)want);
    }
}

void cksum89_test_u64(cksum89_u64 got, cksum89_u32 hi, cksum89_u32 lo,
                      const char *what)
{
    ++cksum89_test_checks;
    if ((got.hi != hi) || (got.lo != lo))
    {
        ++cksum89_test_failures;
        fprintf(stderr, "FAIL: %s: got 0x%08lx%08lx want 0x%08lx%08lx\n", what,
                (unsigned long)got.hi, (unsigned long)got.lo, (unsigned long)hi,
                (unsigned long)lo);
    }
}

void cksum89_test_u16_at(cksum89_u16 got, cksum89_u16 want, const char *what,
                         unsigned long where)
{
    ++cksum89_test_checks;
    if (got != want)
    {
        ++cksum89_test_failures;
        fprintf(stderr, "FAIL: %s at %lu: got 0x%04x want 0x%04x\n", what,
                where, (unsigned int)got, (unsigned int)want);
    }
}

void cksum89_test_u32_at(cksum89_u32 got, cksum89_u32 want, const char *what,
                         unsigned long where)
{
    ++cksum89_test_checks;
    if (got != want)
    {
        ++cksum89_test_failures;
        fprintf(stderr, "FAIL: %s at %lu: got 0x%08lx want 0x%08lx\n", what,
                where, (unsigned long)got, (unsigned long)want);
    }
}

void cksum89_test_u64_at(cksum89_u64 got, cksum89_u32 hi, cksum89_u32 lo,
                         const char *what, unsigned long where)
{
    ++cksum89_test_checks;
    if ((got.hi != hi) || (got.lo != lo))
    {
        ++cksum89_test_failures;
        fprintf(stderr, "FAIL: %s at %lu: got 0x%08lx%08lx want 0x%08lx%08lx\n",
                what, where, (unsigned long)got.hi, (unsigned long)got.lo,
                (unsigned long)hi, (unsigned long)lo);
    }
}

int main(void)
{
    test_vectors();
    test_crc32_iso_hdlc();
    test_crc32c();
    test_crc64_nvme();
    test_inet16();
    test_stream();
    test_tables();
    if (cksum89_test_failures != 0)
    {
        fprintf(stderr, "%d of %d checks FAILED\n", cksum89_test_failures,
                cksum89_test_checks);
        return 1;
    }
    printf("all %d checks passed\n", cksum89_test_checks);
    return 0;
}
