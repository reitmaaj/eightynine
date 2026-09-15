/* test_u64.c - unit tests for the portable 64-bit scalar helpers. */

#include <stdio.h>

#include <raft89.h>

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static raft89_u64 mk(raft89_u32 hi, raft89_u32 lo)
{
    raft89_u64 v;

    v.hi = hi;
    v.lo = lo;
    return v;
}

/* Independent reference comparison over the two words. */
static int ref_cmp(raft89_u64 a, raft89_u64 b)
{
    if (a.hi < b.hi)
    {
        return -1;
    }
    if (a.hi > b.hi)
    {
        return 1;
    }
    if (a.lo < b.lo)
    {
        return -1;
    }
    if (a.lo > b.lo)
    {
        return 1;
    }
    return 0;
}

static void test_zero(void)
{
    raft89_u64 z;

    z = raft89_u64_zero();
    if (z.hi != 0u)
    {
        fail("zero hi");
    }
    if (z.lo != 0u)
    {
        fail("zero lo");
    }
}

static void test_from_u32(void)
{
    raft89_u64 v;

    v = raft89_u64_from_u32(0u);
    if (v.hi != 0u)
    {
        fail("from_u32 0 hi");
    }
    if (v.lo != 0u)
    {
        fail("from_u32 0 lo");
    }
    v = raft89_u64_from_u32(1u);
    if (v.hi != 0u)
    {
        fail("from_u32 1 hi");
    }
    if (v.lo != 1u)
    {
        fail("from_u32 1 lo");
    }
    v = raft89_u64_from_u32(0x7fffffffu);
    if (v.lo != 0x7fffffffu)
    {
        fail("from_u32 2^31-1");
    }
    v = raft89_u64_from_u32(0xffffffffu);
    if (v.hi != 0u)
    {
        fail("from_u32 max hi");
    }
    if (v.lo != 0xffffffffu)
    {
        fail("from_u32 max lo");
    }
}

static void test_matrix(void)
{
    static const raft89_u32 words[7][2] = {{0u, 0u},
                                           {0u, 1u},
                                           {0u, 0xffffffffu},
                                           {1u, 0u},
                                           {1u, 1u},
                                           {0x7fffffffu, 0xffffffffu},
                                           {0xffffffffu, 0xffffffffu}};
    raft89_u64 a;
    raft89_u64 b;
    int got;
    int want;
    int eq;
    int i;
    int j;

    for (i = 0; i < 7; ++i)
    {
        for (j = 0; j < 7; ++j)
        {
            a = mk(words[i][0], words[i][1]);
            b = mk(words[j][0], words[j][1]);
            got = raft89_u64_cmp(a, b);
            want = ref_cmp(a, b);
            if (got != want)
            {
                fail("cmp mismatch");
            }
            eq = raft89_u64_equal(a, b);
            if (eq != (want == 0))
            {
                fail("equal mismatch");
            }
            if (raft89_u64_cmp(b, a) != ref_cmp(b, a))
            {
                fail("cmp antisymmetry");
            }
        }
    }
}

static void test_equality(void)
{
    raft89_u64 a;
    raft89_u64 b;

    a = mk(1u, 2u);
    b = mk(1u, 2u);
    if (!raft89_u64_equal(a, b))
    {
        fail("equal same");
    }
    b = mk(1u, 3u);
    if (raft89_u64_equal(a, b))
    {
        fail("equal lo differs");
    }
    b = mk(2u, 2u);
    if (raft89_u64_equal(a, b))
    {
        fail("equal hi differs");
    }
}

int main(void)
{
    test_zero();
    test_from_u32();
    test_matrix();
    test_equality();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_u64: ok\n");
    return 0;
}
