/* test_regress_trail.c - mutation-trail rollback (D2): conflict restoration,
 * nested checkpoints, commit, and constraint-state reset on rollback. */
#include "fx_fixture.h"

static void run_conflict_restored(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *erow;
    fx_checkpoint cp;
    fx_conflict_kind kind;
    const fx_constraint *p;
    const fx_constraint *s;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &e);
    fx_row_var(ctx, e, &erow);
    CHECK(fx_require_lacks(ctx, a, erow, 0) == FX_OK);
    cp = fx_ctx_checkpoint(ctx);
    CHECK(cp != 0u);
    CHECK(fx_require_member(ctx, a, erow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    CHECK(fx_last_conflict(ctx, &kind, &p, &s) == FX_OK);
    CHECK(kind == FX_CONFLICT_MEMBER_LACKS);
    CHECK(fx_ctx_rollback(ctx, cp) == FX_OK);
    CHECK(fx_last_conflict(ctx, &kind, &p, &s) == FX_OK);
    CHECK(kind == FX_CONFLICT_NONE);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_ctx_free(ctx);
}

static void run_nested_commit(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *erow;
    fx_checkpoint cp1;
    fx_checkpoint cp2;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &e);
    fx_row_var(ctx, e, &erow);
    cp1 = fx_ctx_checkpoint(ctx);
    CHECK(cp1 != 0u);
    CHECK(fx_require_member(ctx, a, erow, 0) == FX_OK);
    cp2 = fx_ctx_checkpoint(ctx);
    CHECK(cp2 != 0u);
    CHECK(fx_require_member(ctx, b, erow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, erow, b) == FX_TRUE);
    CHECK(fx_ctx_rollback(ctx, cp2) == FX_OK);
    CHECK(fx_row_membership(ctx, erow, b) == FX_UNKNOWN);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, erow, a) == FX_TRUE);
    CHECK(fx_row_membership(ctx, erow, b) == FX_UNKNOWN);
    CHECK(fx_ctx_commit(ctx, cp1) == FX_OK);
    CHECK(fx_ctx_rollback(ctx, cp1) == FX_ERR_INVALID);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_conflict_restored();
    run_nested_commit();
    TEST_END
}
