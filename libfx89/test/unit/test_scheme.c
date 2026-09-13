/* test_scheme.c - effect-only schemes: generalize/instantiate/inspect. */
#include "fx_fixture.h"

static void run_explicit(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *body;
    fx_scheme *sch;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &body);
    CHECK(fx_scheme_new(ctx, &e, 1u, body, NULL, 0u, &sch) == FX_OK);
    CHECK(fx_scheme_var_count(sch) == 1u);
    CHECK(fx_scheme_var_id_at(sch, 0) == fx_var_id_of(e));
    CHECK(fx_scheme_body(sch) == body);
    CHECK(fx_scheme_constraint_count(sch) == 0u);
    fx_ctx_free(ctx);
}

static void run_generalize(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *body;
    fx_scheme *sch;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e, &body);
    CHECK(fx_generalize(ctx, body, NULL, 0u, &sch) == FX_OK);
    CHECK(fx_scheme_var_count(sch) == 1u);
    CHECK(fx_scheme_var_id_at(sch, 0) == fx_var_id_of(e));
    fx_ctx_free(ctx);
}

static void run_exclude(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *body;
    fx_scheme *sch;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e, &body);
    CHECK(fx_generalize(ctx, body, &e, 1u, &sch) == FX_OK);
    CHECK(fx_scheme_var_count(sch) == 0u);
    fx_ctx_free(ctx);
}

static void run_instantiate_fresh(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *body;
    fx_scheme *sch;
    const fx_row *r1;
    const fx_row *r2;
    fx_var *t1;
    fx_var *t2;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e, &body);
    CHECK(fx_generalize(ctx, body, NULL, 0u, &sch) == FX_OK);
    CHECK(fx_instantiate(ctx, sch, &r1) == FX_OK);
    CHECK(fx_instantiate(ctx, sch, &r2) == FX_OK);
    CHECK(fx_row_membership(ctx, r1, a) == FX_TRUE);
    CHECK(fx_row_membership(ctx, r2, a) == FX_TRUE);
    t1 = fx_row_tail(ctx, r1);
    t2 = fx_row_tail(ctx, r2);
    CHECK(t1 != NULL);
    CHECK(t2 != NULL);
    if (t1 != NULL && t2 != NULL)
    {
        CHECK(fx_var_id_of(t1) != fx_var_id_of(t2));
        CHECK(fx_var_id_of(t1) != fx_var_id_of(e));
    }
    fx_ctx_free(ctx);
}

static void run_residual_capture(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *vrow;
    const fx_row *body;
    fx_scheme *sch;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_var(ctx, e, &vrow);
    fx_row_open(ctx, atoms, 1u, e, &body);
    CHECK(fx_require_subset(ctx, vrow, body, 0) == FX_OK);
    CHECK(fx_generalize(ctx, vrow, NULL, 0u, &sch) == FX_OK);
    CHECK(fx_scheme_var_count(sch) == 1u);
    CHECK(fx_scheme_constraint_count(sch) == 1u);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_explicit();
    run_generalize();
    run_exclude();
    run_instantiate_fresh();
    run_residual_capture();
    TEST_END
}
