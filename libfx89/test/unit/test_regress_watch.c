/* test_regress_watch.c - precise variable watchers (D4): a binding that
 * exposes a new tail re-registers dependencies, and a fact on one variable
 * wakes every PENDING constraint watching it. */
#include "fx_fixture.h"

static void run_bind_exposes_tail(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e1;
    fx_var *e2;
    fx_var *e3;
    const fx_row *r1;
    const fx_row *r2;
    const fx_row *e3open;
    const fx_row *empty;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_var_new(ctx, &e3);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    fx_row_empty(ctx, &empty);
    fx_row_var(ctx, e3, &e3open);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    /* e2 := {| e3 } (open alias to e3) */
    CHECK(fx_require_equal(ctx, r2, e3open, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, r1, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, e3open, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_multi_consumer(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *src;
    fx_var *d1;
    fx_var *d2;
    const fx_row *srow;
    const fx_row *r1;
    const fx_row *r2;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &src);
    fx_var_new(ctx, &d1);
    fx_var_new(ctx, &d2);
    fx_row_var(ctx, src, &srow);
    fx_row_var(ctx, d1, &r1);
    fx_row_var(ctx, d2, &r2);
    CHECK(fx_require_subset(ctx, srow, r1, 0) == FX_OK);
    CHECK(fx_require_subset(ctx, srow, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_require_member(ctx, a, srow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, r1, a) == FX_TRUE);
    CHECK(fx_row_membership(ctx, r2, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_bind_exposes_tail();
    run_multi_consumer();
    TEST_END
}
