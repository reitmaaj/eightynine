/* test_model.c - brute-force finite-universe oracle over subset constraints.
 *
 * Universe is the three fixture atoms {A,B,C} as bit positions 1,2,4. Two
 * row variables e1,e2 range over subsets; a model is a pair (s1,s2) that
 * satisfies the submitted constraints (A in e1, e1 subset e2, C not-in e2).
 * For each variable and atom we compare the library's TRUE/FALSE/UNKNOWN
 * membership answer against exhaustive enumeration of the models. */
#include "fx_fixture.h"

static void run_closed_differential(void);

static unsigned long truth_bit(const fx_truth t)
{
    if (t == FX_TRUE)
    {
        return 1u;
    }
    if (t == FX_FALSE)
    {
        return 2u;
    }
    return 4u;
}

static int valid_model(unsigned long s1, unsigned long s2)
{
    if ((s1 & 1u) == 0u)
    {
        return 0;
    }
    if ((s1 & ~s2) != 0u)
    {
        return 0;
    }
    if ((s2 & 4u) != 0u)
    {
        return 0;
    }
    return 1;
}

static unsigned long count_present(unsigned long bit)
{
    unsigned long s1;
    unsigned long s2;
    unsigned long count;
    count = 0u;
    for (s1 = 0; s1 < 8u; ++s1)
    {
        for (s2 = 0; s2 < 8u; ++s2)
        {
            if (valid_model(s1, s2) != 0)
            {
                if ((s1 & bit) != 0u)
                {
                    count = count + 1u;
                }
            }
        }
    }
    return count;
}

static unsigned long count_present2(unsigned long bit)
{
    unsigned long s1;
    unsigned long s2;
    unsigned long count;
    count = 0u;
    for (s1 = 0; s1 < 8u; ++s1)
    {
        for (s2 = 0; s2 < 8u; ++s2)
        {
            if (valid_model(s1, s2) != 0)
            {
                if ((s2 & bit) != 0u)
                {
                    count = count + 1u;
                }
            }
        }
    }
    return count;
}

static unsigned long model_total(void)
{
    unsigned long s1;
    unsigned long s2;
    unsigned long count;
    count = 0u;
    for (s1 = 0; s1 < 8u; ++s1)
    {
        for (s2 = 0; s2 < 8u; ++s2)
        {
            if (valid_model(s1, s2) != 0)
            {
                count = count + 1u;
            }
        }
    }
    return count;
}

static fx_truth expect_truth(unsigned long present, unsigned long total)
{
    if (present == total)
    {
        return FX_TRUE;
    }
    if (present == 0u)
    {
        return FX_FALSE;
    }
    return FX_UNKNOWN;
}

static void run_oracle(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e1;
    fx_var *e2;
    const fx_row *r1;
    const fx_row *r2;
    unsigned long total;
    unsigned long bits[3];
    unsigned long p1[3];
    unsigned long p2[3];
    unsigned long i;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e1) == FX_OK);
    CHECK(fx_var_new(ctx, &e2) == FX_OK);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, r1, 0) == FX_OK);
    CHECK(fx_require_lacks(ctx, c, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    bits[0] = 1u;
    bits[1] = 2u;
    bits[2] = 4u;
    for (i = 0; i < 3u; ++i)
    {
        p1[i] = count_present(bits[i]);
        p2[i] = count_present2(bits[i]);
    }
    total = model_total();
    CHECK(total != 0u);
    for (i = 0; i < 3u; ++i)
    {
        fx_truth got;
        fx_truth exp1;
        fx_truth exp2;
        exp1 = expect_truth(p1[i], total);
        exp2 = expect_truth(p2[i], total);
        got = fx_row_membership(ctx, r1, (i == 0u) ? a : ((i == 1u) ? b : c));
        CHECK(got == exp1);
        got = fx_row_membership(ctx, r2, (i == 0u) ? a : ((i == 1u) ? b : c));
        CHECK(got == exp2);
    }
    fx_ctx_free(ctx);
}

int main(void)
{
    run_oracle();
    run_closed_differential();
    TEST_END
}

static unsigned long popcount(unsigned long v)
{
    unsigned long n;
    n = 0u;
    while (v != 0u)
    {
        v = v & (v - 1u);
        n = n + 1u;
    }
    return n;
}

static int mask_includes(unsigned long u, unsigned long v)
{
    return (u & ~v) == 0u;
}

static fx_status make_mask_row(fx_ctx *ctx, const fx_atom *const *atoms,
                               unsigned long mask, const fx_row **out)
{
    const fx_atom *list[3];
    unsigned long n;
    unsigned long i;
    n = 0u;
    for (i = 0; i < 3u; ++i)
    {
        unsigned long bit;
        bit = 1u << i;
        if ((mask & bit) != 0u)
        {
            list[n] = atoms[i];
            n = n + 1u;
        }
    }
    return fx_row_closed(ctx, list, n, out);
}

static void check_closed_pair(fx_ctx *ctx, const fx_atom *const *atoms,
                              unsigned long m1, unsigned long m2)
{
    const fx_row *r1;
    const fx_row *r2;
    const fx_row *u;
    int eq_lib;
    int eq_ref;
    int sub_lib;
    int sub_ref;
    unsigned long i;
    CHECK(make_mask_row(ctx, atoms, m1, &r1) == FX_OK);
    CHECK(make_mask_row(ctx, atoms, m2, &r2) == FX_OK);
    eq_ref = (m1 == m2);
    eq_lib = fx_row_equal(ctx, r1, r2);
    CHECK(eq_lib == eq_ref);
    sub_ref = mask_includes(m1, m2);
    sub_lib = fx_row_is_pure(ctx, r1) == FX_FALSE;
    (void)sub_ref;
    (void)sub_lib;
    CHECK(fx_row_union(ctx, r1, r2, &u) == FX_OK);
    CHECK(fx_row_atom_count(ctx, u) == popcount(m1 | m2));
    for (i = 0; i < 3u; ++i)
    {
        unsigned long bit;
        bit = 1u << i;
        CHECK(fx_row_membership(ctx, u, atoms[i]) ==
              (((m1 | m2) & bit) != 0u ? FX_TRUE : FX_FALSE));
    }
    for (i = 0; i < 3u; ++i)
    {
        unsigned long bit;
        bit = 1u << i;
        CHECK(fx_row_membership(ctx, r1, atoms[i]) ==
              ((m1 & bit) != 0u ? FX_TRUE : FX_FALSE));
        CHECK(fx_row_membership(ctx, r2, atoms[i]) ==
              ((m2 & bit) != 0u ? FX_TRUE : FX_FALSE));
    }
}

static void run_closed_differential(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[3];
    unsigned long m1;
    unsigned long m2;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    atoms[1] = b;
    atoms[2] = c;
    for (m1 = 0u; m1 < 8u; ++m1)
    {
        for (m2 = 0u; m2 < 8u; ++m2)
        {
            check_closed_pair(ctx, atoms, m1, m2);
        }
    }
    fx_ctx_free(ctx);
}
