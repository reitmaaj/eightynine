/* test_u64.c - public scalar helpers and internal 64-bit arithmetic. */

#include "test.h"

#include "ledger89_internal.h"

int main(void)
{
    ledger89_u64 a;
    ledger89_u64 b;
    ledger89_u64 z;
    led89_u32 u32out;
    led89_u64 x;
    led89_u64 sum;
    size_t szout;
    int ok;

    z = ledger89_u64_zero();
    CHECK_EQ(z.hi, 0u);
    CHECK_EQ(z.lo, 0u);

    a = ledger89_u64_from_u32(7u);
    CHECK_EQ(a.hi, 0u);
    CHECK_EQ(a.lo, 7u);

    CHECK_EQ(ledger89_u64_cmp(z, a), -1);
    CHECK_EQ(ledger89_u64_cmp(a, z), 1);
    CHECK_EQ(ledger89_u64_cmp(a, a), 0);
    CHECK(ledger89_u64_equal(z, ledger89_u64_zero()) != 0);
    CHECK(ledger89_u64_equal(a, ledger89_u64_from_u32(7u)) != 0);
    CHECK(ledger89_u64_equal(a, ledger89_u64_from_u32(8u)) == 0);

    /* Comparison must consider the high word first. */
    b.hi = 1u;
    b.lo = 0u;
    a.hi = 0u;
    a.lo = 0xFFFFFFFFu;
    CHECK_EQ(ledger89_u64_cmp(b, a), 1);
    CHECK_EQ(ledger89_u64_cmp(a, b), -1);

    /* Round-trip through the internal representation. */
    x = led89_from_public(b);
    CHECK_EQ(x, ((led89_u64)1 << 32));
    CHECK(ledger89_u64_equal(led89_to_public(x), b) != 0);

    /* Internal addition and overflow detection. */
    ok = led89_u64_add((led89_u64)3, (led89_u64)4, &sum);
    CHECK_EQ(ok, 1);
    CHECK_EQ(sum, (led89_u64)7);
    x = ~(led89_u64)0;
    ok = led89_u64_add(x, (led89_u64)1, &sum);
    CHECK_EQ(ok, 0);
    CHECK_EQ(led89_u64_is_zero((led89_u64)0), 1);
    CHECK_EQ(led89_u64_is_zero((led89_u64)1), 0);

    /* Width conversions. */
    ok = led89_u64_to_u32((led89_u64)0xFFFFFFFFu, &u32out);
    CHECK_EQ(ok, LEDGER89_OK);
    CHECK_EQ(u32out, 0xFFFFFFFFu);
    ok = led89_u64_to_u32(((led89_u64)1 << 32), &u32out);
    CHECK_EQ(ok, LEDGER89_ERANGE);
    ok = led89_u64_to_size((led89_u64)16, &szout);
    CHECK_EQ(ok, LEDGER89_OK);
    CHECK_EQ(szout, 16u);

    TEST_END;
}
