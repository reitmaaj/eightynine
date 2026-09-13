/* test_regress_worklist.c - explicit worklist solving (D3): transitive forward
 * and backward propagation and incremental re-solve after late facts. */
#include "fx_fixture.h"

static void run_forward_transitive(void)
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
    const fx_row *r3;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_var_new(ctx, &e3);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    fx_row_var(ctx, e3, &r3);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_require_subset(ctx, r2, r3, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, r1, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, r3, a) == FX_TRUE);
    CHECK(fx_row_membership(ctx, r3, b) == FX_UNKNOWN);
    fx_ctx_free(ctx);
}

static void run_backward(void)
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
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_require_lacks(ctx, a, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, r1, a) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_incremental(void)
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
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_require_member(ctx, a, r1, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, r2, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_forward_transitive();
    run_backward();
    run_incremental();
    TEST_END
}
