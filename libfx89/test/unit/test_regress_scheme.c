/* test_regress_scheme.c - regression tests for immutable effect-only schemes:
 * no retroactive live-variable capture, required/forbidden/disjoint facts are
 * preserved on instantiation, and closure validation is enforced. */
#include "fx_fixture.h"

static void run_not_retroactive_binding(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *one[1];
    fx_var *e;
    const fx_row *erow;
    const fx_row *arow;
    fx_scheme *sch;
    const fx_row *inst;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &erow);
    CHECK(fx_generalize(ctx, erow, NULL, 0u, &sch) == FX_OK);
    one[0] = a;
    fx_row_closed(ctx, one, 1u, &arow);
    CHECK(fx_require_equal(ctx, erow, arow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_instantiate(ctx, sch, &inst) == FX_OK);
    CHECK(fx_row_membership(ctx, inst, a) == FX_UNKNOWN);
    fx_ctx_free(ctx);
}

static void run_required_preserved(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *erow;
    fx_scheme *sch;
    const fx_row *inst;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &erow);
    CHECK(fx_require_member(ctx, a, erow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_generalize(ctx, erow, NULL, 0u, &sch) == FX_OK);
    CHECK(fx_instantiate(ctx, sch, &inst) == FX_OK);
    CHECK(fx_row_membership(ctx, inst, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_disjoint_head_preserved(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *one[1];
    fx_var *e;
    const fx_row *body;
    fx_scheme *sch;
    const fx_row *inst;
    fx_var *tail;
    const fx_row *trow;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    one[0] = a;
    fx_row_open(ctx, one, 1u, e, &body);
    CHECK(fx_generalize(ctx, body, NULL, 0u, &sch) == FX_OK);
    CHECK(fx_instantiate(ctx, sch, &inst) == FX_OK);
    CHECK(fx_row_membership(ctx, inst, a) == FX_TRUE);
    tail = fx_row_tail(ctx, inst);
    CHECK(tail != NULL);
    if (tail != NULL)
    {
        fx_row_var(ctx, tail, &trow);
        CHECK(fx_require_member(ctx, a, trow, 0) == FX_OK);
        CHECK(fx_solve(ctx) != FX_OK);
    }
    fx_ctx_free(ctx);
}

static void run_explicit_outer_rejected(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    fx_var *f;
    const fx_row *erow;
    const fx_row *frow;
    fx_constraint *sub;
    fx_scheme *sch;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    CHECK(fx_var_new(ctx, &f) == FX_OK);
    fx_row_var(ctx, e, &erow);
    fx_row_var(ctx, f, &frow);
    CHECK(fx_require_subset(ctx, erow, frow, &sub) == FX_OK);
    CHECK(fx_scheme_new(ctx, &e, 1u, erow, &sub, 1u, &sch) == FX_ERR_INVALID);
    fx_ctx_free(ctx);
}

static void run_duplicate_quant_rejected(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    fx_var *qv[2];
    const fx_row *erow;
    fx_scheme *sch;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &erow);
    qv[0] = e;
    qv[1] = e;
    CHECK(fx_scheme_new(ctx, qv, 2u, erow, NULL, 0u, &sch) == FX_ERR_INVALID);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_not_retroactive_binding();
    run_required_preserved();
    run_disjoint_head_preserved();
    run_explicit_outer_rejected();
    run_duplicate_quant_rejected();
    TEST_END
}
