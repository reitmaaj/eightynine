/* test_equal.c - equality solving over open rows. */
#include "fx_fixture.h"

static void run_closed_permute(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    const fx_row *r1;
    const fx_row *r2;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &r1);
    atoms[0] = b;
    atoms[1] = a;
    fx_row_closed(ctx, atoms, 2u, &r2);
    CHECK(fx_require_equal(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_ctx_free(ctx);
}

static void run_closed_mismatch(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    const fx_row *r1;
    const fx_row *r2;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    fx_row_closed(ctx, atoms, 1u, &r1);
    atoms[0] = b;
    fx_row_closed(ctx, atoms, 1u, &r2);
    CHECK(fx_require_equal(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    fx_ctx_free(ctx);
}

static void run_open_to_closed(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    fx_var *e;
    const fx_row *r;
    const fx_row *closed;
    const fx_row *bound;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e, &r);
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &closed);
    CHECK(fx_require_equal(ctx, r, closed, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_var_binding(ctx, e, &bound) == FX_OK);
    CHECK(bound != NULL);
    CHECK(fx_row_atom_count(ctx, bound) == 1u);
    CHECK(fx_row_membership(ctx, bound, b) == FX_TRUE);
    CHECK(fx_row_is_open(ctx, bound) == 0);
    fx_ctx_free(ctx);
}

static void run_empty_residual(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *r;
    const fx_row *closed;
    const fx_row *bound;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e, &r);
    atoms[0] = a;
    fx_row_closed(ctx, atoms, 1u, &closed);
    CHECK(fx_require_equal(ctx, r, closed, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_var_binding(ctx, e, &bound) == FX_OK);
    CHECK(bound != NULL);
    CHECK(fx_row_atom_count(ctx, bound) == 0u);
    fx_ctx_free(ctx);
}

static void run_left_unmatched(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    fx_var *e;
    const fx_row *r;
    const fx_row *closed;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    atoms[1] = c;
    fx_row_open(ctx, atoms, 2u, e, &r);
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &closed);
    CHECK(fx_require_equal(ctx, r, closed, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    fx_ctx_free(ctx);
}

static void run_var_to_closed(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *vr;
    const fx_row *closed;
    const fx_row *bound;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    CHECK(fx_row_var(ctx, e, &vr) == FX_OK);
    atoms[0] = a;
    atoms[0] = b;
    fx_row_closed(ctx, atoms, 1u, &closed);
    CHECK(fx_require_equal(ctx, vr, closed, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_var_binding(ctx, e, &bound) == FX_OK);
    CHECK(bound != NULL);
    CHECK(fx_row_membership(ctx, bound, b) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_open_open(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e1;
    fx_var *e2;
    const fx_row *r1;
    const fx_row *r2;
    const fx_row *b1;
    const fx_row *b2;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e1) == FX_OK);
    CHECK(fx_var_new(ctx, &e2) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e1, &r1);
    atoms[0] = b;
    fx_row_open(ctx, atoms, 1u, e2, &r2);
    CHECK(fx_require_equal(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_var_binding(ctx, e1, &b1) == FX_OK);
    CHECK(fx_var_binding(ctx, e2, &b2) == FX_OK);
    CHECK(b1 != NULL);
    CHECK(b2 != NULL);
    /* e1 := {B|z}, e2 := {A|z}; the full rows both become {A,B|z}. */
    CHECK(fx_row_membership(ctx, b1, b) == FX_TRUE);
    CHECK(fx_row_membership(ctx, b2, a) == FX_TRUE);
    CHECK(fx_row_membership(ctx, r1, a) == FX_TRUE);
    CHECK(fx_row_membership(ctx, r1, b) == FX_TRUE);
    CHECK(fx_row_equal(ctx, r1, r2));
    fx_ctx_free(ctx);
}

static void run_shared_tail_mismatch(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *r1;
    const fx_row *r2;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e, &r1);
    atoms[0] = b;
    fx_row_open(ctx, atoms, 1u, e, &r2);
    CHECK(fx_require_equal(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    fx_ctx_free(ctx);
}

static void run_self_cycle(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *vr;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e, &r);
    CHECK(fx_row_var(ctx, e, &vr) == FX_OK);
    CHECK(fx_require_equal(ctx, vr, r, 0) == FX_OK);
    CHECK(fx_solve(ctx) != FX_OK);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_closed_permute();
    run_closed_mismatch();
    run_open_to_closed();
    run_empty_residual();
    run_left_unmatched();
    run_var_to_closed();
    run_open_open();
    run_shared_tail_mismatch();
    run_self_cycle();
    TEST_END
}
