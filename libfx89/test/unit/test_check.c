/* test_check.c - read-only fx_check_subset / fx_check_lacks queries. */
#include "fx_fixture.h"

static void run_lacks(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    const fx_row *row;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &row);
    CHECK(fx_check_lacks(ctx, c, row) == FX_TRUE);
    CHECK(fx_check_lacks(ctx, a, row) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_subset_closed(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    const fx_row *ra;
    const fx_row *rc;
    const fx_row *rab;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &rab);
    atoms[0] = a;
    fx_row_closed(ctx, atoms, 1u, &ra);
    atoms[0] = c;
    fx_row_closed(ctx, atoms, 1u, &rc);
    CHECK(fx_check_subset(ctx, ra, rab) == FX_TRUE);
    CHECK(fx_check_subset(ctx, rc, rab) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_subset_open_unknown(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e1;
    fx_var *e2;
    const fx_row *r1;
    const fx_row *r2;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e1) == FX_OK);
    CHECK(fx_var_new(ctx, &e2) == FX_OK);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    CHECK(fx_check_subset(ctx, r1, r2) == FX_UNKNOWN);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_lacks();
    run_subset_closed();
    run_subset_open_unknown();
    TEST_END
}
