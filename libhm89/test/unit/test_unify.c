/* test_unify.c - unification: binding, structural, mismatch, occurs. */
#include "test.h"

static hm_type *make_pair(hm_ctx *ctx, hm_type *x, hm_type *y)
{
    return hm_type_app2(ctx, "Pair", x, y);
}

static void run_reflexive(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    st = hm_unify(ctx, a, a, NULL);
    CHECK(st == HM_OK);
    hm_ctx_destroy(ctx);
}

static void run_binding(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *i;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    i = hm_type_const(ctx, "Int");
    st = hm_unify(ctx, a, i, NULL);
    CHECK(st == HM_OK);
    CHECK(hm_type_prune(a) == i);
    hm_ctx_destroy(ctx);
}

static void run_con_equality(void)
{
    hm_ctx *ctx;
    hm_type *l1;
    hm_type *l2;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    l1 = hm_type_app1(ctx, "List", hm_type_const(ctx, "Int"));
    l2 = hm_type_app1(ctx, "List", hm_type_const(ctx, "Int"));
    st = hm_unify(ctx, l1, l2, NULL);
    CHECK(st == HM_OK);
    hm_ctx_destroy(ctx);
}

static void run_con_mismatch(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *b;
    hm_error *error;
    hm_error_kind kind;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    a = hm_type_const(ctx, "Int");
    b = hm_type_const(ctx, "Bool");
    error = NULL;
    st = hm_unify(ctx, a, b, &error);
    CHECK(st == HM_ERROR_MISMATCH);
    CHECK(error != NULL);
    kind = hm_error_kind_of(error);
    CHECK(kind == HM_ERR_MISMATCH);
    CHECK(hm_error_left(error) == a);
    CHECK(hm_error_right(error) == b);
    hm_ctx_destroy(ctx);
}

static void run_arity_mismatch(void)
{
    hm_ctx *ctx;
    hm_type *one;
    hm_type *two;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    one = hm_type_app1(ctx, "T", hm_type_const(ctx, "Int"));
    two = hm_type_app2(ctx, "T", hm_type_const(ctx, "Int"),
                       hm_type_const(ctx, "Bool"));
    st = hm_unify(ctx, one, two, NULL);
    CHECK(st == HM_ERROR_MISMATCH);
    hm_ctx_destroy(ctx);
}

static void run_recursive(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *b;
    hm_type *left;
    hm_type *right;
    hm_type *bool_node;
    hm_type *int_node;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    b = hm_type_var(ctx);
    int_node = hm_type_const(ctx, "Int");
    bool_node = hm_type_const(ctx, "Bool");
    left = make_pair(ctx, a, int_node);
    right = make_pair(ctx, bool_node, b);
    st = hm_unify(ctx, left, right, NULL);
    CHECK(st == HM_OK);
    CHECK(hm_type_prune(a) == bool_node);
    CHECK(hm_type_prune(b) == int_node);
    hm_ctx_destroy(ctx);
}

static void run_occurs(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *list_a;
    hm_error *error;
    hm_error_kind kind;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    list_a = hm_type_app1(ctx, "List", a);
    error = NULL;
    st = hm_unify(ctx, a, list_a, &error);
    CHECK(st == HM_ERROR_OCCURS);
    CHECK(error != NULL);
    kind = hm_error_kind_of(error);
    CHECK(kind == HM_ERR_OCCURS);
    CHECK(hm_error_left(error) == a);
    CHECK(hm_error_right(error) == list_a);
    hm_ctx_destroy(ctx);
}

static void run_occurs_fun(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *fun;
    hm_error *error;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    fun = hm_type_fun(ctx, hm_type_const(ctx, "Int"), a);
    error = NULL;
    st = hm_unify(ctx, a, fun, &error);
    CHECK(st == HM_ERROR_OCCURS);
    CHECK(error != NULL);
    hm_ctx_destroy(ctx);
}

static void run_no_error_node_when_omitted(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *b;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    a = hm_type_const(ctx, "Int");
    b = hm_type_const(ctx, "Bool");
    st = hm_unify(ctx, a, b, NULL);
    CHECK(st == HM_ERROR_MISMATCH);
    hm_ctx_destroy(ctx);
}

int main(void)
{
    run_reflexive();
    run_binding();
    run_con_equality();
    run_con_mismatch();
    run_arity_mismatch();
    run_recursive();
    run_occurs();
    run_occurs_fun();
    run_no_error_node_when_omitted();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
