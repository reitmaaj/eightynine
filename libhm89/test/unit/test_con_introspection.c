/* test_con_introspection.c - public constructor name/arity/arg accessors. */
#include "test.h"

static void run_con_name(void)
{
    hm_ctx *ctx;
    hm_type *c;
    const char *name;
    ctx = hm_ctx_new(NULL);
    c = hm_type_app1(ctx, "List", hm_type_const(ctx, "Int"));
    name = hm_type_con_name(c);
    CHECK(name != NULL);
    if (name != NULL)
    {
        CHECK(strcmp(name, "List") == 0);
    }
    hm_ctx_destroy(ctx);
}

static void run_con_arity(void)
{
    hm_ctx *ctx;
    hm_type *zero;
    hm_type *pair;
    ctx = hm_ctx_new(NULL);
    zero = hm_type_const(ctx, "Void");
    CHECK(hm_type_con_arity(zero) == 0);
    pair = hm_type_app2(ctx, "Pair", hm_type_const(ctx, "Int"),
                        hm_type_const(ctx, "Bool"));
    CHECK(hm_type_con_arity(pair) == 2);
    hm_ctx_destroy(ctx);
}

static void run_con_args(void)
{
    hm_ctx *ctx;
    hm_type *i;
    hm_type *b;
    hm_type *pair;
    hm_type *a0;
    hm_type *a1;
    ctx = hm_ctx_new(NULL);
    i = hm_type_const(ctx, "Int");
    b = hm_type_const(ctx, "Bool");
    pair = hm_type_app2(ctx, "Pair", i, b);
    a0 = hm_type_con_arg(pair, 0);
    a1 = hm_type_con_arg(pair, 1);
    CHECK(a0 == i);
    CHECK(a1 == b);
    hm_ctx_destroy(ctx);
}

static void run_pruned_con(void)
{
    hm_ctx *ctx;
    hm_type *v;
    hm_type *li;
    const char *name;
    ctx = hm_ctx_new(NULL);
    v = hm_type_var(ctx);
    li = hm_type_app1(ctx, "List", hm_type_const(ctx, "Int"));
    CHECK(hm_unify(ctx, v, li, NULL) == HM_OK);
    name = hm_type_con_name(v);
    CHECK(name != NULL);
    if (name != NULL)
    {
        CHECK(strcmp(name, "List") == 0);
    }
    CHECK(hm_type_con_arity(v) == 1);
    CHECK(hm_type_equal(hm_type_con_arg(v, 0), hm_type_const(ctx, "Int")) == 1);
    hm_ctx_destroy(ctx);
}

static void run_non_con(void)
{
    hm_ctx *ctx;
    hm_type *v;
    ctx = hm_ctx_new(NULL);
    v = hm_type_var(ctx);
    CHECK(hm_type_con_name(v) == NULL);
    CHECK(hm_type_con_arity(v) == 0);
    CHECK(hm_type_con_arg(v, 0) == NULL);
    hm_ctx_destroy(ctx);
}

static void run_out_of_range(void)
{
    hm_ctx *ctx;
    hm_type *pair;
    ctx = hm_ctx_new(NULL);
    pair = hm_type_app2(ctx, "Pair", hm_type_const(ctx, "Int"),
                        hm_type_const(ctx, "Bool"));
    CHECK(hm_type_con_arg(pair, 2) == NULL);
    CHECK(hm_type_con_arg(pair, 1) != NULL);
    hm_ctx_destroy(ctx);
}

int main(void)
{
    run_con_name();
    run_con_arity();
    run_con_args();
    run_pruned_con();
    run_non_con();
    run_out_of_range();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
