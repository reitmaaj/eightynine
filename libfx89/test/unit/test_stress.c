/* test_stress.c - moderately large solver scenarios. */
#include "fx_fixture.h"

#define CHAIN_N 200

static void run_subset_chain(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *vars[CHAIN_N];
    const fx_row *rows[CHAIN_N];
    const fx_row *last;
    unsigned long i;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    for (i = 0; i < CHAIN_N; ++i)
    {
        CHECK(fx_var_new(ctx, &vars[i]) == FX_OK);
        CHECK(fx_row_var(ctx, vars[i], &rows[i]) == FX_OK);
    }
    for (i = 0; i + 1u < CHAIN_N; ++i)
    {
        CHECK(fx_require_subset(ctx, rows[i], rows[i + 1u], 0) == FX_OK);
    }
    CHECK(fx_require_member(ctx, a, rows[0], 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    last = rows[CHAIN_N - 1u];
    CHECK(fx_row_membership(ctx, last, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_open_open_many(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    const fx_row *rows[64];
    unsigned long i;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    for (i = 0; i < 64u; ++i)
    {
        fx_var *e1;
        fx_var *e2;
        const fx_row *r1;
        const fx_row *r2;
        CHECK(fx_var_new(ctx, &e1) == FX_OK);
        CHECK(fx_var_new(ctx, &e2) == FX_OK);
        fx_row_var(ctx, e1, &r1);
        atoms[0] = a;
        fx_row_open(ctx, atoms, 1u, e2, &r2);
        CHECK(fx_require_equal(ctx, r1, r2, 0) == FX_OK);
        rows[i] = r1;
    }
    CHECK(fx_solve(ctx) == FX_OK);
    for (i = 0; i < 64u; ++i)
    {
        CHECK(fx_row_membership(ctx, rows[i], a) == FX_TRUE);
    }
    fx_ctx_free(ctx);
}

int main(void)
{
    run_subset_chain();
    run_open_open_many();
    TEST_END
}
