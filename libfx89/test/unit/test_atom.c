/* test_atom.c - nominal atom behavior. */
#include "fx_fixture.h"

static void run_nominal(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(a != NULL);
    CHECK(b != NULL);
    CHECK(fx_atom_kind(a) != 0u);
    CHECK(fx_atom_is_parameterized(a) == 0);
    CHECK(fx_atom_equal(ctx, a, a));
    CHECK(!fx_atom_equal(ctx, a, b));
    CHECK(!fx_atom_equal(ctx, a, c));
    fx_ctx_free(ctx);
}

static void run_interned(void)
{
    fx_ctx *ctx;
    fx_kind_id ka;
    const fx_atom *a1;
    const fx_atom *a2;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "IO", 0, &ka);
    CHECK(fx_atom_nominal(ctx, ka, &a1) == FX_OK);
    CHECK(fx_atom_nominal(ctx, ka, &a2) == FX_OK);
    CHECK(fx_atom_equal(ctx, a1, a2));
    fx_ctx_free(ctx);
}

static void run_unknown_kind(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    fx_ctx_new(&ctx);
    CHECK(fx_atom_nominal(ctx, 0u, &a) == FX_ERR_UNKNOWN);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_nominal();
    run_interned();
    run_unknown_kind();
    TEST_END
}
