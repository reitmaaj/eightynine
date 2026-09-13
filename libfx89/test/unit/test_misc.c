/* test_misc.c - free vars, exact removal, checkpoints, version. */
#include "fx_fixture.h"

static void run_free_vars(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *row;
    fx_var **vs;
    unsigned long n;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e, &row);
    CHECK(fx_row_free_vars(ctx, row, &vs, &n) == FX_OK);
    CHECK(n == 1u);
    if (n == 1u)
    {
        CHECK(fx_var_id_of(vs[0]) == fx_var_id_of(e));
    }
    fx_ctx_free(ctx);
}

static void run_free_vars_closed(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    const fx_row *row;
    fx_var **vs;
    unsigned long n;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    fx_row_closed(ctx, atoms, 1u, &row);
    CHECK(fx_row_free_vars(ctx, row, &vs, &n) == FX_OK);
    CHECK(n == 0u);
    fx_ctx_free(ctx);
}

static void run_remove_closed(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    const fx_row *row;
    const fx_row *out;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &row);
    CHECK(fx_row_remove(ctx, row, a, &out) == FX_OK);
    CHECK(fx_row_atom_count(ctx, out) == 1u);
    CHECK(fx_row_membership(ctx, out, b) == FX_TRUE);
    CHECK(fx_row_membership(ctx, out, a) == FX_FALSE);
    fx_ctx_free(ctx);
}

static void run_remove_absent(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    const fx_row *row;
    const fx_row *out;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &row);
    CHECK(fx_row_remove(ctx, row, c, &out) == FX_OK);
    CHECK(fx_row_atom_count(ctx, out) == 2u);
    fx_ctx_free(ctx);
}

static void run_remove_open_explicit(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[2];
    fx_var *e;
    const fx_row *row;
    const fx_row *out;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    atoms[1] = b;
    fx_row_open(ctx, atoms, 2u, e, &row);
    CHECK(fx_row_remove(ctx, row, a, &out) == FX_OK);
    CHECK(fx_row_atom_count(ctx, out) == 1u);
    CHECK(fx_row_membership(ctx, out, b) == FX_TRUE);
    CHECK(fx_row_is_open(ctx, out));
    fx_ctx_free(ctx);
}

static void run_remove_unsupported(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[1];
    fx_var *e;
    const fx_row *row;
    const fx_row *out;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    atoms[0] = a;
    fx_row_open(ctx, atoms, 1u, e, &row);
    CHECK(fx_row_remove(ctx, row, b, &out) == FX_ERR_UNSUPPORTED);
    fx_ctx_free(ctx);
}

static void run_remove_many(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom *atoms[3];
    const fx_atom *drop[2];
    const fx_row *row;
    const fx_row *out;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms[0] = a;
    atoms[1] = b;
    atoms[2] = c;
    fx_row_closed(ctx, atoms, 3u, &row);
    drop[0] = a;
    drop[1] = b;
    CHECK(fx_row_remove_many(ctx, row, drop, 2u, &out) == FX_OK);
    CHECK(fx_row_atom_count(ctx, out) == 1u);
    CHECK(fx_row_membership(ctx, out, c) == FX_TRUE);
    fx_ctx_free(ctx);
}

static void run_version(void)
{
    CHECK(fx_version_major() == FX89_VERSION_MAJOR);
    CHECK(fx_version_minor() == FX89_VERSION_MINOR);
    CHECK(fx_version_patch() == FX89_VERSION_PATCH);
}

static void run_checkpoint_fact_rollback(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_var *e;
    const fx_row *erow;
    fx_checkpoint cp;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &erow);
    cp = fx_ctx_checkpoint(ctx);
    CHECK(cp != 0u);
    CHECK(fx_require_member(ctx, a, erow, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_row_membership(ctx, erow, a) == FX_TRUE);
    CHECK(fx_ctx_rollback(ctx, cp) == FX_OK);
    CHECK(fx_row_membership(ctx, erow, a) == FX_UNKNOWN);
    fx_ctx_free(ctx);
}

static void run_checkpoint_binding_rollback(void)
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
    fx_checkpoint cp;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_var_new(ctx, &e) == FX_OK);
    fx_row_var(ctx, e, &vr);
    atoms[0] = a;
    fx_row_closed(ctx, atoms, 1u, &closed);
    cp = fx_ctx_checkpoint(ctx);
    CHECK(fx_require_equal(ctx, vr, closed, 0) == FX_OK);
    CHECK(fx_solve(ctx) == FX_OK);
    CHECK(fx_var_binding(ctx, e, &bound) == FX_OK);
    CHECK(bound != NULL);
    CHECK(fx_ctx_rollback(ctx, cp) == FX_OK);
    CHECK(fx_var_binding(ctx, e, &bound) == FX_OK);
    CHECK(bound == NULL);
    fx_ctx_free(ctx);
}

static void run_checkpoint_nesting(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_checkpoint cp1;
    fx_checkpoint cp2;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    cp1 = fx_ctx_checkpoint(ctx);
    cp2 = fx_ctx_checkpoint(ctx);
    CHECK(cp2 != 0u);
    CHECK(fx_ctx_rollback(ctx, cp1) == FX_ERR_INVALID);
    CHECK(fx_ctx_rollback(ctx, cp2) == FX_OK);
    CHECK(fx_ctx_commit(ctx, cp1) == FX_OK);
    CHECK(fx_ctx_commit(ctx, cp1) == FX_ERR_INVALID);
    fx_ctx_free(ctx);
}

static void run_checkpoint_decl_persist(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    fx_kind_id kind;
    fx_checkpoint cp;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    cp = fx_ctx_checkpoint(ctx);
    CHECK(fx_kind_define(ctx, "K", 0, &kind) == FX_OK);
    CHECK(fx_ctx_rollback(ctx, cp) == FX_OK);
    CHECK(fx_kind_name(ctx, kind) != NULL);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_free_vars();
    run_free_vars_closed();
    run_remove_closed();
    run_remove_absent();
    run_remove_open_explicit();
    run_remove_unsupported();
    run_remove_many();
    run_version();
    run_checkpoint_fact_rollback();
    run_checkpoint_binding_rollback();
    run_checkpoint_nesting();
    run_checkpoint_decl_persist();
    TEST_END
}
