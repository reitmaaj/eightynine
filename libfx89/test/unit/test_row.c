/* test_row.c - row construction, normalization, membership. */
#include "fx_fixture.h"

static void run_empty(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_row *e;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_row_empty(ctx, &e) == FX_OK);
    CHECK(fx_row_atom_count(ctx, e) == 0u);
    CHECK(fx_row_is_open(ctx, e) == 0);
    CHECK(fx_row_membership(ctx, e, a) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_canonicalize(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[5];
    const fx_row *row;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = c;
    atoms[1] = a;
    atoms[2] = a;
    atoms[3] = b;
    atoms[4] = a;
    CHECK(fx_row_closed(ctx, atoms, 5u, &row) == FX_OK);
    CHECK(fx_row_atom_count(ctx, row) == 3u);
    CHECK(fx_row_membership(ctx, row, a) == FX_TRUE);
    CHECK(fx_row_membership(ctx, row, b) == FX_TRUE);
    CHECK(fx_row_membership(ctx, row, c) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_variable_row(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *row;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    CHECK(fx_row_var(ctx, e, &row) == FX_OK);
    CHECK(fx_row_atom_count(ctx, row) == 0u);
    CHECK(fx_row_is_open(ctx, row));
    CHECK(fx_row_tail(ctx, row) == e);
    CHECK(fx_row_membership(ctx, row, a) == FX_UNKNOWN);
    fx_ctx_free(ctx);
}

static void run_open_tail(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_atom *atoms[1];
    const fx_row *row;
    const fx_row *erow;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    CHECK(fx_row_open(ctx, atoms, 1u, e, &row) == FX_OK);
    CHECK(fx_row_is_open(ctx, row));
    CHECK(fx_row_atom_count(ctx, row) == 1u);
    CHECK(fx_row_membership(ctx, row, a) == FX_TRUE);
    CHECK(fx_row_membership(ctx, row, b) == FX_UNKNOWN);
    /* Disjoint tail: a is forbidden from e, so membership in e is FALSE. */
    CHECK(fx_row_var(ctx, e, &erow) == FX_OK);
    CHECK(fx_row_membership(ctx, erow, a) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_extend(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_row *e;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_row_empty(ctx, &e) == FX_OK);
    CHECK(fx_row_extend(ctx, e, a, &r) == FX_OK);
    CHECK(fx_row_atom_count(ctx, r) == 1u);
    CHECK(fx_row_membership(ctx, r, a) == FX_TRUE);
    CHECK(fx_row_extend(ctx, r, a, &r) == FX_OK);
    CHECK(fx_row_atom_count(ctx, r) == 1u);
    CHECK(fx_row_extend(ctx, r, b, &r) == FX_OK);
    CHECK(fx_row_atom_count(ctx, r) == 2u);
    fx_ctx_free(ctx);
}

static void run_open_extend(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_atom *atoms[1];
    const fx_row *row;
    const fx_row *r;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    CHECK(fx_row_open(ctx, atoms, 1u, e, &row) == FX_OK);
    CHECK(fx_row_extend(ctx, row, b, &r) == FX_OK);
    CHECK(fx_row_atom_count(ctx, r) == 2u);
    CHECK(fx_row_is_open(ctx, r));
    fx_ctx_free(ctx);
}

int main(void)
{
    run_empty();
    run_canonicalize();
    run_variable_row();
    run_open_tail();
    run_extend();
    run_open_extend();
    TEST_END
}
