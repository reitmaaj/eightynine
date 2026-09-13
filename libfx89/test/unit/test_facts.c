/* test_facts.c - membership, lacks, and purity constraints and queries. */
#include "fx_fixture.h"

static void run_member_closed(void)
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
    CHECK(fx_require_member(ctx, a, row, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_ctx_free(ctx);
}

static void run_member_closed_absent(void)
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
    CHECK(fx_require_member(ctx, c, row, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    fx_ctx_free(ctx);
}

static void run_member_open_prop(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    fx_var *e;
    const fx_row *row;
    const fx_row *erow;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    atoms[1] = b;
    fx_row_open(ctx, atoms, 2u, e, &row);
    CHECK(fx_require_member(ctx, c, row, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_row_var(ctx, e, &erow);
    CHECK(fx_row_membership(ctx, erow, c) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_member_tail_contradiction(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *row;
    const fx_row *erow;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e, &row);
    fx_row_var(ctx, e, &erow);
    CHECK(fx_require_member(ctx, a, erow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    fx_ctx_free(ctx);
}

static void run_lacks_closed(void)
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
    CHECK(fx_require_lacks(ctx, c, row, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    fx_ctx_free(ctx);
}

static void run_lacks_present(void)
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
    CHECK(fx_require_lacks(ctx, a, row, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    fx_ctx_free(ctx);
}

static void run_fact_contradiction(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *erow;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &erow);
    CHECK(fx_require_member(ctx, a, erow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_require_lacks(ctx, a, erow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    fx_ctx_free(ctx);
}

static void run_fact_reverse(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *erow;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &erow);
    CHECK(fx_require_lacks(ctx, a, erow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_require_member(ctx, a, erow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    fx_ctx_free(ctx);
}

static void run_pure_queries(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *empty;
    const fx_row *row;
    const fx_row *vr;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    fx_row_empty(ctx, &empty);
    atoms[0] = a;
    fx_row_closed(ctx, atoms, 1u, &row);
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &vr);
    CHECK(fx_row_is_pure(ctx, empty) == FX_TRUE);
    CHECK(fx_row_is_pure(ctx, row) == FX_FALSE);
    CHECK(fx_row_is_pure(ctx, vr) == FX_UNKNOWN);
    fx_ctx_free(ctx);
}

static void run_require_pure_binds(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *vr;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &vr);
    CHECK(fx_require_pure(ctx, vr, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_is_pure(ctx, vr) == FX_TRUE);
    CHECK(fx_row_membership(ctx, vr, a) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_require_pure_contradiction(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *vr;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &vr);
    CHECK(fx_require_member(ctx, a, vr, 0) == FX_OK);
    CHECK(fx_require_pure(ctx, vr, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_ERR_UNSAT);
    fx_ctx_free(ctx);
}

static void run_closed_queries(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *row;
    const fx_row *orow;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    fx_row_closed(ctx, atoms, 1u, &row);
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e, &orow);
    CHECK(fx_row_is_closed(ctx, row) == FX_TRUE);
    CHECK(fx_row_is_closed(ctx, orow) == FX_UNKNOWN);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_member_closed();
    run_member_closed_absent();
    run_member_open_prop();
    run_member_tail_contradiction();
    run_lacks_closed();
    run_lacks_present();
    run_fact_contradiction();
    run_fact_reverse();
    run_pure_queries();
    run_require_pure_binds();
    run_require_pure_contradiction();
    run_closed_queries();
    TEST_END
}
