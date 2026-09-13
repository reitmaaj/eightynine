/* test_subset.c - inclusion/subset constraints and residual propagation. */
#include "fx_fixture.h"

static void run_prop_after_lhs_bind(void);

static void run_closed_cases(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    const fx_row *s;
    const fx_row *t;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &s);
    fx_row_closed(ctx, atoms, 2u, &t);
    CHECK(fx_require_subset(ctx, s, t, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_ctx_free(ctx);
}

static void run_empty_subset(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    const fx_row *e;
    const fx_row *s;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_row_empty(ctx, &e);
    atoms[0] = a;
    fx_row_closed(ctx, atoms, 1u, &s);
    CHECK(fx_require_subset(ctx, e, s, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_ctx_free(ctx);
}

static void run_counter_example(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    const fx_row *s;
    const fx_row *t;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    atoms[1] = c;
    fx_row_closed(ctx, atoms, 2u, &s);
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &t);
    CHECK(fx_require_subset(ctx, s, t, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    fx_ctx_free(ctx);
}

static void run_open_target_prop(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *lhs[2];
    const fx_atom *rhs[1];
    fx_var *e;
    const fx_row *ls;
    const fx_row *rs;
    const fx_row *erow;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    lhs[0] = a;
    lhs[1] = b;
    fx_row_closed(ctx, lhs, 2u, &ls);
    rhs[0] = a;
    fx_row_open(ctx, rhs, 1u, e, &rs);
    CHECK(fx_require_subset(ctx, ls, rs, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_row_var(ctx, e, &erow);
    CHECK(fx_row_membership(ctx, erow, b) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_common_atom_ignored(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *lhs[1];
    const fx_atom *rhs[1];
    fx_var *e;
    const fx_row *ls;
    const fx_row *rs;
    const fx_row *erow;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    lhs[0] = a;
    fx_row_closed(ctx, lhs, 1u, &ls);
    rhs[0] = a;
    fx_row_open(ctx, rhs, 1u, e, &rs);
    CHECK(fx_require_subset(ctx, ls, rs, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_row_var(ctx, e, &erow);
    CHECK(fx_row_membership(ctx, erow, a) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_forward(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e1;
    fx_var *e2;
    const fx_row *r1;
    const fx_row *r2;
    const fx_row *e2row;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e1) == FX_OK);
    CHECK(fx_var_new(ctx, &e2) == FX_OK);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, r1, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_row_var(ctx, e2, &e2row);
    CHECK(fx_row_membership(ctx, e2row, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_future_forward(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e1;
    fx_var *e2;
    const fx_row *r1;
    const fx_row *r2;
    const fx_row *e2row;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e1) == FX_OK);
    CHECK(fx_var_new(ctx, &e2) == FX_OK);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_require_member(ctx, a, r1, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_row_var(ctx, e2, &e2row);
    CHECK(fx_row_membership(ctx, e2row, a) == FX_TRUE);
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
    const fx_row *e1row;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e1) == FX_OK);
    CHECK(fx_var_new(ctx, &e2) == FX_OK);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_require_lacks(ctx, a, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_row_var(ctx, e1, &e1row);
    CHECK(fx_row_membership(ctx, e1row, a) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_future_backward(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e1;
    fx_var *e2;
    const fx_row *r1;
    const fx_row *r2;
    const fx_row *e1row;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e1) == FX_OK);
    CHECK(fx_var_new(ctx, &e2) == FX_OK);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_require_lacks(ctx, a, r2, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_row_var(ctx, e1, &e1row);
    CHECK(fx_row_membership(ctx, e1row, a) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_closed_upper_bound(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    fx_var *e;
    const fx_row *vr;
    const fx_row *ub;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &vr);
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &ub);
    CHECK(fx_require_subset(ctx, vr, ub, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_require_member(ctx, a, vr, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_require_member(ctx, c, vr, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    fx_ctx_free(ctx);
}

static void run_transitive_forward(void)
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
    const fx_row *e3row;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e1) == FX_OK);
    CHECK(fx_var_new(ctx, &e2) == FX_OK);
    CHECK(fx_var_new(ctx, &e3) == FX_OK);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    fx_row_var(ctx, e3, &r3);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_require_subset(ctx, r2, r3, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, r1, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_row_var(ctx, e3, &e3row);
    CHECK(fx_row_membership(ctx, e3row, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_transitive_backward(void)
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
    const fx_row *e1row;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e1) == FX_OK);
    CHECK(fx_var_new(ctx, &e2) == FX_OK);
    CHECK(fx_var_new(ctx, &e3) == FX_OK);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    fx_row_var(ctx, e3, &r3);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    CHECK(fx_require_subset(ctx, r2, r3, 0) == FX_OK);
    CHECK(fx_require_lacks(ctx, a, r3, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_row_var(ctx, e1, &e1row);
    CHECK(fx_row_membership(ctx, e1row, a) == FX_FALSE);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_closed_cases();
    run_empty_subset();
    run_counter_example();
    run_open_target_prop();
    run_common_atom_ignored();
    run_forward();
    run_future_forward();
    run_backward();
    run_future_backward();
    run_closed_upper_bound();
    run_transitive_forward();
    run_transitive_backward();
    run_prop_after_lhs_bind();
    TEST_END
}

static void run_prop_after_lhs_bind(void)
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
    const fx_row *closed;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e1) == FX_OK);
    CHECK(fx_var_new(ctx, &e2) == FX_OK);
    fx_row_var(ctx, e1, &r1);
    fx_row_var(ctx, e2, &r2);
    CHECK(fx_require_subset(ctx, r1, r2, 0) == FX_OK);
    atoms[0] = a;
    fx_row_closed(ctx, atoms, 1u, &closed);
    CHECK(fx_require_equal(ctx, r1, closed, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    /* e1 = {A} and e1 subset e2 force A in e2. */
    CHECK(fx_row_membership(ctx, r2, a) == FX_TRUE);
    fx_ctx_free(ctx);
}
