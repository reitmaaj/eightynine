/* test_regress_ownership.c - regression tests for ownership hardening:
 * cross-context objects are rejected, the atom domain is validated and
 * copied by value, and mixed-context atom equality is refused. */
#include "fx_fixture.h"

static void make_ctx(fx_ctx **ctx, const fx_atom **a)
{
    fx_kind_id ka;
    CHECK(fx_ctx_new(ctx) == FX_OK);
    CHECK(fx_kind_define(*ctx, "K", 0, &ka) == FX_OK);
    CHECK(fx_atom_nominal(*ctx, ka, a) == FX_OK);
}

static void run_cross_context_require(void)
{
    fx_ctx *ctxa;
    fx_ctx *ctxb;
    const fx_atom *aa;
    const fx_atom *ba;
    const fx_row *emptyb;
    const fx_row *arowb;
    make_ctx(&ctxa, &aa);
    make_ctx(&ctxb, &ba);
    fx_row_empty(ctxb, &emptyb);
    fx_row_closed(ctxb, &ba, 1u, &arowb);
    CHECK(fx_require_member(ctxa, ba, arowb, 0) == FX_ERR_INVALID);
    CHECK(fx_require_equal(ctxa, emptyb, arowb, 0) == FX_ERR_INVALID);
    fx_ctx_free(ctxb);
    fx_ctx_free(ctxa);
}

static void run_cross_context_atom_equal(void)
{
    fx_ctx *ctxa;
    fx_ctx *ctxb;
    const fx_atom *aa;
    const fx_atom *ba;
    make_ctx(&ctxa, &aa);
    make_ctx(&ctxb, &ba);
    CHECK(fx_atom_equal(ctxa, aa, ba) == 0);
    fx_ctx_free(ctxb);
    fx_ctx_free(ctxa);
}

static int stub_cmp(void *u, fx_kind_id k, const void *x, const void *y)
{
    (void)u;
    (void)k;
    (void)x;
    (void)y;
    return 0;
}

static fx_status stub_copy(void *u, fx_kind_id k, const void *v, void **o)
{
    (void)u;
    (void)k;
    (void)v;
    *o = NULL;
    return FX_OK;
}

static void stub_destroy(void *u, fx_kind_id k, void *v)
{
    (void)u;
    (void)k;
    (void)v;
}

static void run_domain_incomplete_rejected(void)
{
    fx_ctx *ctx;
    fx_atom_domain no_copy;
    fx_atom_domain no_destroy;
    CHECK(fx_ctx_new(&ctx) == FX_OK);
    no_copy.compare = stub_cmp;
    no_copy.copy = NULL;
    no_copy.destroy = stub_destroy;
    no_copy.hash = NULL;
    CHECK(fx_ctx_set_atom_domain(ctx, &no_copy, 0) == FX_ERR_INVALID);
    no_destroy.compare = stub_cmp;
    no_destroy.copy = stub_copy;
    no_destroy.destroy = NULL;
    no_destroy.hash = NULL;
    CHECK(fx_ctx_set_atom_domain(ctx, &no_destroy, 0) == FX_ERR_INVALID);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_cross_context_require();
    run_cross_context_atom_equal();
    run_domain_incomplete_rejected();
    TEST_END
}
