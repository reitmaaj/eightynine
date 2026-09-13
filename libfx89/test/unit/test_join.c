/* test_join.c - exact set union: closed simplification and residual JOIN. */
#include "fx_fixture.h"

static void run_closed_union(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    const fx_row *x;
    const fx_row *y;
    const fx_row *u;
    const fx_row *e;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_row_empty(ctx, &e);
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &x);
    atoms[0] = b;
    atoms[1] = c;
    fx_row_closed(ctx, atoms, 2u, &y);
    CHECK(fx_row_union(ctx, e, x, &u) == FX_OK);
    CHECK(fx_row_membership(ctx, u, a) == FX_TRUE);
    CHECK(fx_row_membership(ctx, u, b) == FX_TRUE);
    CHECK(fx_row_union(ctx, x, y, &u) == FX_OK);
    CHECK(fx_row_atom_count(ctx, u) == 3u);
    CHECK(fx_row_membership(ctx, u, c) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_overlap(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    const fx_row *x;
    const fx_row *y;
    const fx_row *u;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &x);
    atoms[0] = b;
    atoms[1] = a;
    fx_row_closed(ctx, atoms, 2u, &y);
    fx_row_union(ctx, x, y, &u);
    CHECK(fx_row_atom_count(ctx, u) == 2u);
    fx_ctx_free(ctx);
}

static void run_forward_left(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *zo;
    fx_var *e1;
    fx_var *e2;
    const fx_row *o;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &zo);
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, zo, &o);
    fx_row_var(ctx, e1, &l);
    fx_row_var(ctx, e2, &r);
    CHECK(fx_require_join(ctx, o, l, r, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, l, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, o, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_forward_right(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *zo;
    fx_var *e1;
    fx_var *e2;
    const fx_row *o;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &zo);
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, zo, &o);
    fx_row_var(ctx, e1, &l);
    fx_row_var(ctx, e2, &r);
    CHECK(fx_require_join(ctx, o, l, r, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, r, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, o, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_future_member(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *zo;
    fx_var *e1;
    fx_var *e2;
    const fx_row *o;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &zo);
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, zo, &o);
    fx_row_var(ctx, e1, &l);
    fx_row_var(ctx, e2, &r);
    CHECK(fx_require_join(ctx, o, l, r, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_require_member(ctx, a, l, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, o, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_negative_output(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *zo;
    fx_var *e1;
    fx_var *e2;
    const fx_row *o;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &zo);
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, zo, &o);
    fx_row_var(ctx, e1, &l);
    fx_row_var(ctx, e2, &r);
    CHECK(fx_require_join(ctx, o, l, r, 0) == FX_OK);
    CHECK(fx_require_lacks(ctx, a, o, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, l, a) == FX_FALSE);
    CHECK(fx_row_membership(ctx, r, a) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_both_inputs_lack(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *zo;
    fx_var *e1;
    fx_var *e2;
    const fx_row *o;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &zo);
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, zo, &o);
    fx_row_var(ctx, e1, &l);
    fx_row_var(ctx, e2, &r);
    CHECK(fx_require_join(ctx, o, l, r, 0) == FX_OK);
    CHECK(fx_require_lacks(ctx, a, l, 0) == FX_OK);
    CHECK(fx_require_lacks(ctx, a, r, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, o, a) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_reverse_left_lacks(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *zo;
    fx_var *e1;
    fx_var *e2;
    const fx_row *o;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &zo);
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, zo, &o);
    fx_row_var(ctx, e1, &l);
    fx_row_var(ctx, e2, &r);
    CHECK(fx_require_join(ctx, o, l, r, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, o, 0) == FX_OK);
    CHECK(fx_require_lacks(ctx, a, l, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, r, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_reverse_right_lacks(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *zo;
    fx_var *e1;
    fx_var *e2;
    const fx_row *o;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &zo);
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, zo, &o);
    fx_row_var(ctx, e1, &l);
    fx_row_var(ctx, e2, &r);
    CHECK(fx_require_join(ctx, o, l, r, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, o, 0) == FX_OK);
    CHECK(fx_require_lacks(ctx, a, r, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, l, a) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_underdetermined(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *zo;
    fx_var *e1;
    fx_var *e2;
    const fx_row *o;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &zo);
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, zo, &o);
    fx_row_var(ctx, e1, &l);
    fx_row_var(ctx, e2, &r);
    CHECK(fx_require_join(ctx, o, l, r, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, o, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, l, a) == FX_UNKNOWN);
    CHECK(fx_row_membership(ctx, r, a) == FX_UNKNOWN);
    fx_ctx_free(ctx);
}

static void run_three_models(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *zo;
    fx_var *e1;
    fx_var *e2;
    const fx_row *o;
    const fx_row *l;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &zo);
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, zo, &o);
    fx_row_var(ctx, e1, &l);
    fx_row_var(ctx, e2, &r);
    CHECK(fx_require_join(ctx, o, l, r, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, o, 0) == FX_OK);
    CHECK(fx_require_member(ctx, a, l, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_ctx_free(ctx);
}

static void run_row_union_symbolic(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e1;
    fx_var *e2;
    const fx_row *l;
    const fx_row *r;
    const fx_row *u;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_var_new(ctx, &e1);
    fx_var_new(ctx, &e2);
    fx_row_var(ctx, e1, &l);
    fx_row_var(ctx, e2, &r);
    CHECK(fx_row_union(ctx, l, r, &u) == FX_OK);
    CHECK(fx_require_member(ctx, a, l, 0) == FX_OK);
    CHECK(fx_require_member(ctx, b, r, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, u, a) == FX_TRUE);
    CHECK(fx_row_membership(ctx, u, b) == FX_TRUE);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_closed_union();
    run_overlap();
    run_forward_left();
    run_forward_right();
    run_future_member();
    run_negative_output();
    run_both_inputs_lack();
    run_reverse_left_lacks();
    run_reverse_right_lacks();
    run_underdetermined();
    run_three_models();
    run_row_union_symbolic();
    TEST_END
}
