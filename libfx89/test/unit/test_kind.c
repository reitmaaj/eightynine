/* test_kind.c - effect-kind registry behavior. */
#include <string.h>

#include "fx_fixture.h"

static void run_ids(void)
{
    fx_ctx *ctx;
    fx_kind_id a;
    fx_kind_id b;
    fx_kind_id c;
    fx_ctx_new(&ctx);
    CHECK(fx_kind_define(ctx, "A", 0, &a) == FX_OK);
    CHECK(fx_kind_define(ctx, "B", 0, &b) == FX_OK);
    CHECK(fx_kind_define(ctx, "C", 0, &c) == FX_OK);
    CHECK(a != 0u);
    CHECK(a < b);
    CHECK(b < c);
    fx_ctx_free(ctx);
}

static void run_lookup(void)
{
    fx_ctx *ctx;
    fx_kind_id a;
    fx_kind_id found;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "IO", 0, &a);
    CHECK(fx_kind_lookup(ctx, "IO", &found) == FX_OK);
    CHECK(found == a);
    CHECK(fx_kind_lookup(ctx, "Missing", &found) == FX_ERR_UNKNOWN);
    fx_ctx_free(ctx);
}

static void run_duplicate(void)
{
    fx_ctx *ctx;
    fx_kind_id a;
    fx_kind_id b;
    fx_ctx_new(&ctx);
    CHECK(fx_kind_define(ctx, "IO", 0, &a) == FX_OK);
    CHECK(fx_kind_define(ctx, "IO", 0, &b) == FX_ERR_DUPLICATE);
    fx_ctx_free(ctx);
}

static void run_name_copied(void)
{
    fx_ctx *ctx;
    fx_kind_id id;
    char buf[4];
    fx_ctx_new(&ctx);
    buf[0] = 'I';
    buf[1] = 'O';
    buf[2] = '\0';
    fx_kind_define(ctx, buf, 0, &id);
    buf[0] = 'X';
    CHECK(strcmp(fx_kind_name(ctx, id), "IO") == 0);
    fx_ctx_free(ctx);
}

static void run_userdata(void)
{
    fx_ctx *ctx;
    fx_kind_id id;
    int marker;
    fx_ctx_new(&ctx);
    marker = 7;
    fx_kind_define(ctx, "IO", &marker, &id);
    CHECK(fx_kind_userdata(ctx, id) == &marker);
    CHECK(fx_kind_name(ctx, 0u) == NULL);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_ids();
    run_lookup();
    run_duplicate();
    run_name_copied();
    run_userdata();
    TEST_END
}
