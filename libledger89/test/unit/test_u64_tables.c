/* test_u64_tables.c - table-driven white-box coverage for the internal
 * 64-bit scalar helpers, capacity growth, and min/max helpers. */

#include "test.h"

#include "ledger89_internal.h"

#include "ledger89_file.c"
#include "ledger89_recover.c"
#include "ledger89_util.c"

struct add_case
{
    led89_u64 a;
    led89_u64 b;
    int ok;
    led89_u64 sum;
};

static const struct add_case add_cases[] = {
    {(led89_u64)0, (led89_u64)0, 1, (led89_u64)0},
    {(led89_u64)0, (led89_u64)1, 1, (led89_u64)1},
    {(led89_u64)1, (led89_u64)2, 1, (led89_u64)3},
    {(led89_u64)0xFFFFFFFFu, (led89_u64)1, 1, ((led89_u64)1 << 32)},
    {((led89_u64)1 << 32), (led89_u64)0xFFFFFFFFu, 1,
     (((led89_u64)1 << 33) - (led89_u64)1)},
    {((led89_u64)1 << 63), ((led89_u64)1 << 63), 0, (led89_u64)0},
    {((led89_u64)1 << 63), (((led89_u64)1 << 63) - (led89_u64)1), 1,
     ~((led89_u64)0)},
    {~((led89_u64)0), (led89_u64)0, 1, ~((led89_u64)0)},
    {~((led89_u64)0), (led89_u64)1, 0, (led89_u64)0},
    {~((led89_u64)0), ~((led89_u64)0), 0, (led89_u64)0}};

struct unary_case
{
    led89_u64 in;
    led89_u64 out;
};

static const struct unary_case inc_cases[] = {
    {(led89_u64)0, (led89_u64)1},
    {(led89_u64)1, (led89_u64)2},
    {(led89_u64)41, (led89_u64)42},
    {~((led89_u64)0) - (led89_u64)1, ~((led89_u64)0)},
    {~((led89_u64)0), (led89_u64)0}};

static const struct unary_case dec_cases[] = {
    {(led89_u64)0, ~((led89_u64)0)},
    {(led89_u64)1, (led89_u64)0},
    {(led89_u64)2, (led89_u64)1},
    {(led89_u64)42, (led89_u64)41},
    {~((led89_u64)0), ~((led89_u64)0) - (led89_u64)1}};

static const led89_u64 order_values[] = {
    (led89_u64)0,         (led89_u64)1,
    (led89_u64)2,         (led89_u64)0xFFFFFFFFu,
    ((led89_u64)1 << 32), ((led89_u64)1 << 32) + (led89_u64)1,
    ((led89_u64)1 << 63), ~((led89_u64)0) - (led89_u64)1,
    ~((led89_u64)0)};

struct zero_case
{
    led89_u64 v;
    int expected;
};

static const struct zero_case zero_cases[] = {{(led89_u64)0, 1},
                                              {(led89_u64)1, 0},
                                              {(led89_u64)0xFFFFFFFFu, 0},
                                              {((led89_u64)1 << 32), 0},
                                              {~((led89_u64)0), 0}};

struct to_u32_case
{
    led89_u64 v;
    int expected;
    led89_u32 out;
};

static const struct to_u32_case to_u32_cases[] = {
    {(led89_u64)0, LEDGER89_OK, 0u},
    {(led89_u64)1, LEDGER89_OK, 1u},
    {(led89_u64)0xFFFFFFFFu, LEDGER89_OK, 0xFFFFFFFFu},
    {((led89_u64)1 << 32), LEDGER89_ERANGE, 0u},
    {~((led89_u64)0), LEDGER89_ERANGE, 0u}};

static const led89_u64 to_size_values[] = {
    (led89_u64)0,           (led89_u64)1,         (led89_u64)16,
    (led89_u64)0xFFFFFFFFu, ((led89_u64)1 << 32), ((led89_u64)1 << 63),
    ~((led89_u64)0)};

static const size_t size_values[] = {0u, 1u, 16u, 0xFFFFFFFFu, (size_t)-1};

struct pair_case
{
    led89_u64 a;
    led89_u64 b;
    led89_u64 expected;
};

static const struct pair_case left_cases[] = {
    {(led89_u64)0, (led89_u64)0, (led89_u64)0},
    {(led89_u64)5, (led89_u64)0, (led89_u64)5},
    {(led89_u64)5, (led89_u64)3, (led89_u64)2},
    {~((led89_u64)0), (led89_u64)1, ~((led89_u64)0) - (led89_u64)1},
    {~((led89_u64)0), ~((led89_u64)0), (led89_u64)0},
    {(led89_u64)1, (led89_u64)1, (led89_u64)0}};

static const struct pair_case at_add_cases[] = {
    {(led89_u64)0, (led89_u64)0, (led89_u64)0},
    {(led89_u64)1, (led89_u64)2, (led89_u64)3},
    {~((led89_u64)0), (led89_u64)0, ~((led89_u64)0)},
    {~((led89_u64)0), (led89_u64)1, (led89_u64)0},
    {~((led89_u64)0) - (led89_u64)1, (led89_u64)1, ~((led89_u64)0)},
    {~((led89_u64)0), ~((led89_u64)0), ~((led89_u64)0) - (led89_u64)1}};

struct chunk_case
{
    led89_u64 v;
    size_t expected;
};

static const struct chunk_case chunk_cases[] = {
    {(led89_u64)0, 0u},       {(led89_u64)1, 1u},
    {(led89_u64)4095, 4095u}, {(led89_u64)4096, 4096u},
    {(led89_u64)4097, 4096u}, {~((led89_u64)0), 4096u}};

struct public_case
{
    led89_u32 hi;
    led89_u32 lo;
};

static const struct public_case public_cases[] = {{0u, 0u},
                                                  {0u, 1u},
                                                  {0u, 0xFFFFFFFFu},
                                                  {1u, 0u},
                                                  {0x11223344u, 0x55667788u},
                                                  {0xFFFFFFFFu, 0xFFFFFFFFu}};

struct cap_case
{
    size_t cap;
    size_t need;
    size_t initial;
    size_t expected;
};

static const struct cap_case next_cap_cases[] = {
    {0u, 5u, 8u, 8u},      {0u, 100u, 8u, 100u},   {0u, 64u, 64u, 64u},
    {8u, 9u, 8u, 16u},     {8u, 17u, 8u, 17u},     {1u, 2u, 1u, 2u},
    {64u, 65u, 64u, 128u}, {100u, 100u, 64u, 200u}};

static const struct cap_case double_cases[] = {{0u, 0u, 0u, 0u},
                                               {1u, 0u, 0u, 2u},
                                               {2u, 0u, 0u, 4u},
                                               {100u, 0u, 0u, 200u},
                                               {4096u, 0u, 0u, 8192u}};

static const struct cap_case max_size_cases[] = {
    {0u, 0u, 0u, 0u}, {0u, 1u, 0u, 1u}, {1u, 0u, 0u, 1u},
    {1u, 1u, 0u, 1u}, {5u, 3u, 0u, 5u}, {3u, 5u, 0u, 5u}};

static const struct cap_case size_add_cases[] = {
    {0u, 0u, 0u, 0u}, {1u, 2u, 0u, 3u}, {100u, 200u, 0u, 300u}};

static const struct cap_case size_left_cases[] = {{0u, 0u, 0u, 0u},
                                                  {5u, 3u, 0u, 2u},
                                                  {5u, 5u, 0u, 0u},
                                                  {300u, 100u, 0u, 200u}};

static const struct pair_case max_id_cases[] = {
    {(led89_u64)0, (led89_u64)0, (led89_u64)0},
    {(led89_u64)1, (led89_u64)2, (led89_u64)2},
    {(led89_u64)2, (led89_u64)1, (led89_u64)2},
    {(led89_u64)5, (led89_u64)5, (led89_u64)5},
    {~((led89_u64)0), (led89_u64)0, ~((led89_u64)0)}};

struct max_ref_case
{
    led89_u64 active;
    led89_u32 sealed_count;
    led89_u64 sealed_ids[3];
    led89_u64 expected;
};

static const struct max_ref_case max_ref_cases[] = {
    {(led89_u64)5, 0u, {0u, 0u, 0u}, (led89_u64)5},
    {(led89_u64)5, 3u, {2u, 9u, 3u}, (led89_u64)9},
    {(led89_u64)7, 2u, {2u, 3u, 0u}, (led89_u64)7},
    {(led89_u64)1, 1u, {4u, 0u, 0u}, (led89_u64)4}};

static void test_add(void)
{
    size_t i;

    for (i = 0u; i < sizeof add_cases / sizeof add_cases[0]; ++i)
    {
        const struct add_case *c;
        led89_u64 sum;
        int ok;

        c = &add_cases[i];
        sum = (led89_u64)0x5A5A5A5Au;
        ok = led89_u64_add(c->a, c->b, &sum);
        CHECK_EQ(ok, c->ok);
        if (c->ok != 0)
        {
            CHECK(sum == c->sum);
        }
        else
        {
            CHECK(sum == (led89_u64)0x5A5A5A5Au);
        }
    }
}

static void test_inc_dec(void)
{
    size_t i;

    for (i = 0u; i < sizeof inc_cases / sizeof inc_cases[0]; ++i)
    {
        CHECK(led89_u64_inc(inc_cases[i].in) == inc_cases[i].out);
    }
    for (i = 0u; i < sizeof dec_cases / sizeof dec_cases[0]; ++i)
    {
        CHECK(led89_u64_dec(dec_cases[i].in) == dec_cases[i].out);
    }
}

static void test_cmp_order(void)
{
    size_t i;
    size_t j;

    for (i = 0u; i < sizeof order_values / sizeof order_values[0]; ++i)
    {
        for (j = 0u; j < sizeof order_values / sizeof order_values[0]; ++j)
        {
            int expected;

            expected = 0;
            if (i < j)
            {
                expected = -1;
            }
            if (i > j)
            {
                expected = 1;
            }
            CHECK_EQ(led89_u64_cmp(order_values[i], order_values[j]), expected);
        }
    }
}

static void test_is_zero(void)
{
    size_t i;

    for (i = 0u; i < sizeof zero_cases / sizeof zero_cases[0]; ++i)
    {
        CHECK_EQ(led89_u64_is_zero(zero_cases[i].v), zero_cases[i].expected);
    }
}

static void test_to_u32(void)
{
    size_t i;

    for (i = 0u; i < sizeof to_u32_cases / sizeof to_u32_cases[0]; ++i)
    {
        const struct to_u32_case *c;
        led89_u32 out;
        int rc;

        c = &to_u32_cases[i];
        out = 0u;
        rc = led89_u64_to_u32(c->v, &out);
        CHECK_EQ(rc, c->expected);
        if (rc == LEDGER89_OK)
        {
            CHECK_EQ(out, c->out);
        }
    }
}

static void test_width_conversions(void)
{
    size_t i;

    for (i = 0u; i < sizeof to_size_values / sizeof to_size_values[0]; ++i)
    {
        led89_u64 v;
        size_t out;
        int expected;
        int rc;

        v = to_size_values[i];
        out = 0u;
        expected = LEDGER89_ERANGE;
        if ((led89_u64)(size_t)v == v)
        {
            expected = LEDGER89_OK;
        }
        rc = led89_u64_to_size(v, &out);
        CHECK_EQ(rc, expected);
        if (rc == LEDGER89_OK)
        {
            CHECK(out == (size_t)v);
        }
    }
    for (i = 0u; i < sizeof size_values / sizeof size_values[0]; ++i)
    {
        led89_u64 out;

        out = (led89_u64)0;
        CHECK_EQ(led89_size_to_u64(size_values[i], &out), LEDGER89_OK);
        CHECK(out == (led89_u64)size_values[i]);
    }
}

static void test_left_and_add(void)
{
    size_t i;

    for (i = 0u; i < sizeof left_cases / sizeof left_cases[0]; ++i)
    {
        CHECK(led89_u64_left(left_cases[i].a, left_cases[i].b) ==
              left_cases[i].expected);
    }
    for (i = 0u; i < sizeof at_add_cases / sizeof at_add_cases[0]; ++i)
    {
        CHECK(led89_at_add(at_add_cases[i].a, at_add_cases[i].b) ==
              at_add_cases[i].expected);
    }
}

static void test_chunk_of(void)
{
    size_t i;

    for (i = 0u; i < sizeof chunk_cases / sizeof chunk_cases[0]; ++i)
    {
        CHECK_EQ(led89_chunk_of(chunk_cases[i].v), chunk_cases[i].expected);
    }
}

static void test_public_roundtrip(void)
{
    size_t i;

    for (i = 0u; i < sizeof public_cases / sizeof public_cases[0]; ++i)
    {
        ledger89_u64 p;
        ledger89_u64 back;
        led89_u64 x;
        led89_u64 expected;

        p.hi = public_cases[i].hi;
        p.lo = public_cases[i].lo;
        expected = ((led89_u64)p.hi << 32) | (led89_u64)p.lo;
        x = led89_from_public(p);
        CHECK(x == expected);
        back = led89_to_public(x);
        CHECK_EQ(back.hi, p.hi);
        CHECK_EQ(back.lo, p.lo);
    }
}

static void test_growth(void)
{
    size_t i;

    for (i = 0u; i < sizeof next_cap_cases / sizeof next_cap_cases[0]; ++i)
    {
        CHECK_EQ(led89_next_cap(next_cap_cases[i].cap, next_cap_cases[i].need,
                                next_cap_cases[i].initial),
                 next_cap_cases[i].expected);
    }
    for (i = 0u; i < sizeof double_cases / sizeof double_cases[0]; ++i)
    {
        CHECK_EQ(led89_double_size(double_cases[i].cap),
                 double_cases[i].expected);
    }
    for (i = 0u; i < sizeof max_size_cases / sizeof max_size_cases[0]; ++i)
    {
        CHECK_EQ(led89_max_size(max_size_cases[i].cap, max_size_cases[i].need),
                 max_size_cases[i].expected);
    }
    for (i = 0u; i < sizeof size_add_cases / sizeof size_add_cases[0]; ++i)
    {
        CHECK_EQ(led89_size_add(size_add_cases[i].cap, size_add_cases[i].need),
                 size_add_cases[i].expected);
    }
    for (i = 0u; i < sizeof size_left_cases / sizeof size_left_cases[0]; ++i)
    {
        CHECK_EQ(
            led89_size_left(size_left_cases[i].cap, size_left_cases[i].need),
            size_left_cases[i].expected);
    }
}

static void test_min_max_helpers(void)
{
    size_t i;

    CHECK(led89_r_min() == (led89_u64)LED89_PART_HEADER_SIZE);
    for (i = 0u; i < sizeof max_id_cases / sizeof max_id_cases[0]; ++i)
    {
        CHECK(led89_max_id(max_id_cases[i].a, max_id_cases[i].b) ==
              max_id_cases[i].expected);
    }
    for (i = 0u; i < sizeof max_ref_cases / sizeof max_ref_cases[0]; ++i)
    {
        const struct max_ref_case *c;
        led89_part_desc descs[3];
        led89_manifest m;
        led89_u32 j;

        c = &max_ref_cases[i];
        for (j = 0u; j < 3u; ++j)
        {
            descs[j].file_id = c->sealed_ids[j];
            descs[j].first = (led89_u64)0;
            descs[j].end = (led89_u64)0;
        }
        m.active.file_id = c->active;
        m.active.first = (led89_u64)0;
        m.active.end = (led89_u64)0;
        m.sealed_count = c->sealed_count;
        m.sealed = descs;
        CHECK(led89_max_ref_id(&m) == c->expected);
    }
}

int main(void)
{
    test_add();
    test_inc_dec();
    test_cmp_order();
    test_is_zero();
    test_to_u32();
    test_width_conversions();
    test_left_and_add();
    test_chunk_of();
    test_public_roundtrip();
    test_growth();
    test_min_max_helpers();
    TEST_END;
}
