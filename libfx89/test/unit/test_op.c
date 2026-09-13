/* test_op.c - operation metadata registry behavior. */
#include <string.h>

#include "fx_fixture.h"

static void run_define(void)
{
    fx_ctx *ctx;
    fx_kind_id file;
    fx_kind_id sock;
    fx_op_id op;
    int marker;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "File", 0, &file);
    fx_kind_define(ctx, "Socket", 0, &sock);
    marker = 9;
    CHECK(fx_op_define(ctx, file, "read", &marker, &op) == FX_OK);
    CHECK(op != 0u);
    CHECK(fx_op_kind(ctx, op) == file);
    CHECK(strcmp(fx_op_name(ctx, op), "read") == 0);
    CHECK(fx_op_userdata(ctx, op) == &marker);
    fx_ctx_free(ctx);
}

static void run_duplicate_in_kind(void)
{
    fx_ctx *ctx;
    fx_kind_id file;
    fx_op_id a;
    fx_op_id b;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "File", 0, &file);
    CHECK(fx_op_define(ctx, file, "read", 0, &a) == FX_OK);
    CHECK(fx_op_define(ctx, file, "read", 0, &b) == FX_ERR_DUPLICATE);
    fx_ctx_free(ctx);
}

static void run_cross_kind(void)
{
    fx_ctx *ctx;
    fx_kind_id file;
    fx_kind_id sock;
    fx_op_id a;
    fx_op_id b;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "File", 0, &file);
    fx_kind_define(ctx, "Socket", 0, &sock);
    CHECK(fx_op_define(ctx, file, "read", 0, &a) == FX_OK);
    CHECK(fx_op_define(ctx, sock, "read", 0, &b) == FX_OK);
    CHECK(fx_op_kind(ctx, a) == file);
    CHECK(fx_op_kind(ctx, b) == sock);
    fx_ctx_free(ctx);
}

static void run_unknown_kind(void)
{
    fx_ctx *ctx;
    fx_op_id a;
    fx_ctx_new(&ctx);
    CHECK(fx_op_define(ctx, 0u, "read", 0, &a) == FX_ERR_UNKNOWN);
    CHECK(fx_op_kind(ctx, 0u) == 0u);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_define();
    run_duplicate_in_kind();
    run_cross_kind();
    run_unknown_kind();
    TEST_END
}
