/* test_misuse.c - invalid-input handling and fail-safe behavior. */
#include "fx_fixture.h"

static void run_null_handling(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    CHECK(fx_require_equal(NULL, 0, 0, 0) == FX_ERR_INVALID);
    CHECK(fx_require_subset(NULL, 0, 0, 0) == FX_ERR_INVALID);
    CHECK(fx_require_member(NULL, 0, 0, 0) == FX_ERR_INVALID);
    CHECK(fx_require_lacks(NULL, 0, 0, 0) == FX_ERR_INVALID);
    CHECK(fx_require_pure(NULL, 0, 0) == FX_ERR_INVALID);
    CHECK(fx_solve(NULL) == FX_ERR_INVALID);
    CHECK(fx_ctx_new(NULL) == FX_ERR_INVALID);
    CHECK(fx_row_atom_count(NULL, 0) == 0u);
    CHECK(fx_row_membership(NULL, 0, a) == FX_UNKNOWN);
    CHECK(fx_kind_name(NULL, 0) == NULL);
    fx_ctx_free(NULL);
    fx_ctx_free(ctx);
}

static void run_invalid_kind(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    fx_kind_id id;
    fx_ctx_new(&ctx);
    CHECK(fx_atom_nominal(ctx, 0u, &a) == FX_ERR_UNKNOWN);
    CHECK(fx_op_define(ctx, 0u, "x", 0, &id) == FX_ERR_UNKNOWN);
    CHECK(fx_kind_name(ctx, 0u) == NULL);
    fx_ctx_free(ctx);
}

static void run_invalid_ids(void)
{
    fx_ctx *ctx;
    fx_ctx_new(&ctx);
    CHECK(fx_kind_userdata(ctx, 99999u) == NULL);
    CHECK(fx_op_kind(ctx, 99999u) == 0u);
    CHECK(fx_op_name(ctx, 99999u) == NULL);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_null_handling();
    run_invalid_kind();
    run_invalid_ids();
    TEST_END
}
