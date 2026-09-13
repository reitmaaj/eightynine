/* test_invariants.c - regression guards for core inference invariants. */
#include <stdlib.h>

#include "test.h"

static void run_occurs_leaves_unbound(void)
{
    hm_ctx *ctx;
    hm_error *error;
    hm_status st;
    hm_type *a;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    error = NULL;
    st = hm_unify(ctx, a, hm_type_fun(ctx, a, hm_type_const(ctx, "Int")),
                  &error);
    CHECK(st == HM_ERROR_OCCURS);
    CHECK(error != NULL);
    CHECK(hm_error_left(error) == a);
    CHECK(hm_type_prune(a) == a);
    CHECK(hm_type_kind_of(a) == HM_TYPE_VAR);
    hm_ctx_destroy(ctx);
}

static void run_shared_dag_occurs(void)
{
    hm_ctx *ctx;
    hm_error *error;
    hm_status st;
    hm_type *a;
    hm_type *wrap;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    wrap = hm_type_app1(ctx, "List", hm_type_app2(ctx, "Pair", a, a));
    error = NULL;
    st = hm_unify(ctx, a, wrap, &error);
    CHECK(st == HM_ERROR_OCCURS);
    CHECK(error != NULL);
    hm_ctx_destroy(ctx);
}

static void run_deep_alias_prune(void)
{
    hm_ctx *ctx;
    enum
    {
        N = 3000
    };
    hm_type *first;
    hm_type *cur;
    hm_type *target;
    int i;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    first = hm_type_var(ctx);
    cur = first;
    for (i = 0; i < N; ++i)
    {
        hm_type *next = hm_type_var(ctx);
        st = hm_unify(ctx, cur, next, NULL);
        CHECK(st == HM_OK);
        cur = next;
    }
    target = hm_type_const(ctx, "Int");
    st = hm_unify(ctx, cur, target, NULL);
    CHECK(st == HM_OK);
    CHECK(hm_type_prune(first) == target);
    hm_ctx_destroy(ctx);
}

static void run_single_quant_consistent(void)
{
    hm_ctx *ctx;
    hm_error *error;
    hm_status st;
    hm_type *a;
    hm_type *body;
    hm_type *q[1];
    hm_type *inst;
    hm_type *expect;
    hm_scheme *s;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    body = hm_type_fun(ctx, a, hm_type_fun(ctx, a, hm_type_const(ctx, "Int")));
    q[0] = a;
    s = hm_scheme_new(ctx, 1, q, body);
    CHECK(s != NULL);
    inst = hm_instantiate(ctx, s);
    CHECK(inst != NULL);
    expect = hm_type_fun(ctx, hm_type_const(ctx, "Int"),
                         hm_type_fun(ctx, hm_type_const(ctx, "Bool"),
                                     hm_type_const(ctx, "Int")));
    error = NULL;
    st = hm_unify(ctx, inst, expect, &error);
    CHECK(st == HM_ERROR_MISMATCH);
    hm_ctx_destroy(ctx);
}

static void run_two_quant_independent(void)
{
    hm_ctx *ctx;
    hm_error *error;
    hm_status st;
    hm_type *a;
    hm_type *b;
    hm_type *q[2];
    hm_type *inst;
    hm_type *expect;
    hm_scheme *s;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    b = hm_type_var(ctx);
    q[0] = a;
    q[1] = b;
    s = hm_scheme_new(ctx, 2, q, hm_type_fun(ctx, a, b));
    CHECK(s != NULL);
    inst = hm_instantiate(ctx, s);
    CHECK(inst != NULL);
    expect =
        hm_type_fun(ctx, hm_type_const(ctx, "Int"), hm_type_const(ctx, "Bool"));
    error = NULL;
    st = hm_unify(ctx, inst, expect, &error);
    CHECK(st == HM_OK);
    hm_ctx_destroy(ctx);
}

static void run_parent_mono_blocks_child_gen(void)
{
    hm_ctx *ctx;
    hm_env *parent;
    hm_env *child;
    hm_scheme *mono;
    hm_scheme *s;
    hm_status st;
    hm_type *a;
    hm_type *b;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    parent = hm_env_new(ctx);
    mono = hm_scheme_mono(ctx, a);
    st = hm_env_bind(parent, "x", mono);
    CHECK(st == HM_OK);
    child = hm_env_child(ctx, parent);
    b = hm_type_var(ctx);
    s = hm_generalize(ctx, child, hm_type_fun(ctx, a, b));
    CHECK(s != NULL);
    CHECK(hm_scheme_quantified_count(s) == 1);
    hm_ctx_destroy(ctx);
}

static void run_poly_binding_is_opaque(void)
{
    hm_ctx *ctx;
    hm_env *env;
    hm_env *env2;
    hm_scheme *poly;
    hm_scheme *s;
    hm_status st;
    hm_type *a;
    hm_type *p;
    hm_type *q;
    ctx = hm_ctx_new(NULL);
    env = hm_env_new(ctx);
    a = hm_type_var(ctx);
    poly = hm_generalize(ctx, env, hm_type_fun(ctx, a, a));
    CHECK(poly != NULL);
    env2 = hm_env_new(ctx);
    st = hm_env_bind(env2, "id", poly);
    CHECK(st == HM_OK);
    p = hm_type_var(ctx);
    q = hm_type_var(ctx);
    s = hm_generalize(ctx, env2, hm_type_fun(ctx, p, q));
    CHECK(s != NULL);
    CHECK(hm_scheme_quantified_count(s) == 2);
    hm_ctx_destroy(ctx);
}

int main(void)
{
    run_occurs_leaves_unbound();
    run_shared_dag_occurs();
    run_deep_alias_prune();
    run_single_quant_consistent();
    run_two_quant_independent();
    run_parent_mono_blocks_child_gen();
    run_poly_binding_is_opaque();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
