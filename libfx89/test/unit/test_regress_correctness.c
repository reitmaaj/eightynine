/* test_regress_correctness.c - regression tests for the P0/P1 correctness
 * pass: exact JOIN with closed inputs, transactional rollback of constraint
 * progress, purity/query proofs. */
#include "fx_fixture.h"

static void run_join_open_output_success(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *one[1];
    fx_var *z;
    const fx_row *out;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &z);
    one[0] = a;
    fx_row_open(ctx, one, 1u, z, &out);
    one[0] = a;
    fx_row_closed(ctx, one, 1u, &l);
    one[0] = b;
    fx_row_closed(ctx, one, 1u, &r);
    CHECK(fx_require_join(ctx, out, l, r, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, out, a) == FX_TRUE);
    CHECK(fx_row_membership(ctx, out, b) == FX_TRUE);
    CHECK(fx_row_membership(ctx, out, c) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_join_open_output_mismatch(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *one[1];
    fx_var *z;
    const fx_row *out;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &z);
    one[0] = c;
    fx_row_open(ctx, one, 1u, z, &out);
    one[0] = a;
    fx_row_closed(ctx, one, 1u, &l);
    one[0] = b;
    fx_row_closed(ctx, one, 1u, &r);
    CHECK(fx_require_join(ctx, out, l, r, 0) == FX_OK);
    CHECK(fx_solve(ctx) != FX_OK);
    fx_ctx_free(ctx);
}

static void run_join_closed_output(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *one[1];
    const fx_atom *two[2];
    const fx_row *out;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    two[0] = a;
    two[1] = b;
    fx_row_closed(ctx, two, 2u, &out);
    one[0] = a;
    fx_row_closed(ctx, one, 1u, &l);
    one[0] = b;
    fx_row_closed(ctx, one, 1u, &r);
    CHECK(fx_require_join(ctx, out, l, r, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_ctx_free(ctx);
}

static void run_rollback_repropagates(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *erow;
    fx_checkpoint cp;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &e);
    fx_row_var(ctx, e, &erow);
    CHECK(fx_require_member(ctx, a, erow, 0) == FX_OK);
    cp = fx_ctx_checkpoint(ctx);
    CHECK(cp != 0u);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, erow, a) == FX_TRUE);
    CHECK(fx_ctx_rollback(ctx, cp) == FX_OK);
    CHECK(fx_row_membership(ctx, erow, a) == FX_UNKNOWN);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, erow, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_purity_required_tail(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *erow;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &e);
    fx_row_var(ctx, e, &erow);
    CHECK(fx_row_is_pure(ctx, erow) == FX_UNKNOWN);
    CHECK(fx_require_member(ctx, a, erow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, erow, a) == FX_TRUE);
    CHECK(fx_row_is_pure(ctx, erow) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_subset_reflexive_open(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *one[1];
    fx_var *e;
    const fx_row *erow;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &e);
    one[0] = a;
    fx_row_open(ctx, one, 1u, e, &erow);
    CHECK(fx_check_subset(ctx, erow, erow) == FX_TRUE);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_join_open_output_success();
    run_join_open_output_mismatch();
    run_join_closed_output();
    run_rollback_repropagates();
    run_purity_required_tail();
    run_subset_reflexive_open();
    TEST_END
}
