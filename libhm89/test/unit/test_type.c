/* test_type.c - variables, constructors, pruning, and equality. */
#include "test.h"

static void run_freshness(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *b;
    unsigned long ia;
    unsigned long ib;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    b = hm_type_var(ctx);
    CHECK(a != b);
    ia = hm_type_var_id(a);
    ib = hm_type_var_id(b);
    CHECK(ia != ib);
    hm_ctx_destroy(ctx);
}

static void run_kinds(void)
{
    hm_ctx *ctx;
    hm_type *v;
    hm_type *c;
    hm_type *con;
    ctx = hm_ctx_new(NULL);
    v = hm_type_var(ctx);
    c = hm_type_const(ctx, "Int");
    CHECK(hm_type_kind_of(v) == HM_TYPE_VAR);
    CHECK(hm_type_kind_of(c) == HM_TYPE_CON);
    con = hm_type_app1(ctx, "List", v);
    CHECK(hm_type_kind_of(con) == HM_TYPE_CON);
    CHECK(hm_type_var_id(c) == 0);
    hm_ctx_destroy(ctx);
}

static void run_prune_unbound(void)
{
    hm_ctx *ctx;
    hm_type *a;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    CHECK(hm_type_prune(a) == a);
    hm_ctx_destroy(ctx);
}

static void run_prune_after_bind(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *c;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    c = hm_type_const(ctx, "Int");
    st = hm_unify(ctx, a, c, NULL);
    CHECK(st == HM_OK);
    CHECK(hm_type_prune(a) == c);
    hm_ctx_destroy(ctx);
}

static void run_equal(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *b;
    hm_type *i1;
    hm_type *i2;
    hm_type *l1;
    hm_type *l2;
    ctx = hm_ctx_new(NULL);
    i1 = hm_type_const(ctx, "Int");
    i2 = hm_type_const(ctx, "Int");
    CHECK(hm_type_equal(i1, i2) == 1);
    CHECK(hm_type_equal(i1, hm_type_const(ctx, "Bool")) == 0);
    a = hm_type_var(ctx);
    b = hm_type_var(ctx);
    CHECK(hm_type_equal(a, a) == 1);
    CHECK(hm_type_equal(a, b) == 0);
    l1 = hm_type_app1(ctx, "List", i1);
    l2 = hm_type_app1(ctx, "List", hm_type_const(ctx, "Int"));
    CHECK(hm_type_equal(l1, l2) == 1);
    CHECK(hm_type_equal(l1, hm_type_app1(ctx, "List", a)) == 0);
    hm_ctx_destroy(ctx);
}

static void run_const_arity_via_equal(void)
{
    hm_ctx *ctx;
    hm_type *zero;
    ctx = hm_ctx_new(NULL);
    zero = hm_type_const(ctx, "T");
    CHECK(hm_type_kind_of(zero) == HM_TYPE_CON);
    hm_ctx_destroy(ctx);
}

int main(void)
{
    run_freshness();
    run_kinds();
    run_prune_unbound();
    run_prune_after_bind();
    run_equal();
    run_const_arity_via_equal();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
