/* test_ctx.c - context lifecycle and isolation. */
#include "fx_fixture.h"

static void run_lifecycle(void)
{
    fx_ctx *ctx;
    fx_ctx *ctx2;
    fx_kind_id k1a;
    fx_kind_id k1b;
    fx_kind_id k2a;
    fx_kind_id found;
    CHECK(fx_ctx_new(&ctx) == FX_OK);
    CHECK(fx_kind_define(ctx, "IO", 0, &k1a) == FX_OK);
    CHECK(fx_kind_define(ctx, "State", 0, &k1b) == FX_OK);
    CHECK(fx_ctx_new(&ctx2) == FX_OK);
    CHECK(fx_kind_define(ctx2, "IO", 0, &k2a) == FX_OK);
    /* Each context starts ids at 1 and keeps its own registry. */
    CHECK(k1a == 1u);
    CHECK(k2a == 1u);
    CHECK(fx_kind_lookup(ctx2, "State", &found) == FX_ERR_UNKNOWN);
    CHECK(fx_kind_lookup(ctx, "State", &found) == FX_OK);
    fx_ctx_free(ctx2);
    fx_ctx_free(ctx);
}

static void run_free_null(void)
{
    fx_ctx_free(NULL);
}

static void run_ctx_new_null(void)
{
    CHECK(fx_ctx_new(NULL) == FX_ERR_INVALID);
}

int main(void)
{
    run_lifecycle();
    run_free_null();
    run_ctx_new_null();
    TEST_END
}
