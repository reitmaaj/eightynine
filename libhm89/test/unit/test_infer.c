/* test_infer.c - mini Algorithm W over primitives: end-to-end inference. */
#include <stdlib.h>

#include "test.h"

enum ekind
{
    E_VAR,
    E_LAM,
    E_APP,
    E_LET,
    E_LIT,
    E_PAIR
};

struct expr
{
    int kind;
    const char *name;
    struct expr *a;
    struct expr *b;
};

static struct expr *enode(int kind, const char *name, struct expr *a,
                          struct expr *b)
{
    struct expr *e;
    e = malloc(sizeof(struct expr));
    if (e == NULL)
    {
        return NULL;
    }
    e->kind = kind;
    e->name = name;
    e->a = a;
    e->b = b;
    return e;
}

static hm_status infer(hm_ctx *ctx, hm_env *env, struct expr *e, hm_type **out,
                       hm_error **err)
{
    hm_type *tf;
    hm_type *tx;
    hm_type *tr;
    hm_type *param;
    hm_type *body_type;
    hm_type *fun_type;
    hm_type *pair_type;
    hm_type *t1;
    hm_type *t2;
    hm_scheme *scheme;
    hm_scheme *mono;
    hm_env *child;
    hm_status st;
    if (e->kind == E_VAR)
    {
        scheme = hm_env_lookup(env, e->name);
        if (scheme == NULL)
        {
            return HM_ERROR_UNBOUND;
        }
        *out = hm_instantiate(ctx, scheme);
        return HM_OK;
    }
    if (e->kind == E_LIT)
    {
        *out = hm_type_const(ctx, e->name);
        return HM_OK;
    }
    if (e->kind == E_LAM)
    {
        param = hm_type_var(ctx);
        child = hm_env_child(ctx, env);
        mono = hm_scheme_mono(ctx, param);
        st = hm_env_bind(child, e->name, mono);
        if (st != HM_OK)
        {
            return st;
        }
        st = infer(ctx, child, e->a, &body_type, err);
        if (st != HM_OK)
        {
            return st;
        }
        *out = hm_type_fun(ctx, param, body_type);
        return HM_OK;
    }
    if (e->kind == E_APP)
    {
        st = infer(ctx, env, e->a, &tf, err);
        if (st != HM_OK)
        {
            return st;
        }
        st = infer(ctx, env, e->b, &tx, err);
        if (st != HM_OK)
        {
            return st;
        }
        tr = hm_type_var(ctx);
        fun_type = hm_type_fun(ctx, tx, tr);
        st = hm_unify(ctx, tf, fun_type, err);
        if (st != HM_OK)
        {
            return st;
        }
        *out = tr;
        return HM_OK;
    }
    if (e->kind == E_LET)
    {
        st = infer(ctx, env, e->a, &tf, err);
        if (st != HM_OK)
        {
            return st;
        }
        scheme = hm_generalize(ctx, env, tf);
        child = hm_env_child(ctx, env);
        st = hm_env_bind(child, e->name, scheme);
        if (st != HM_OK)
        {
            return st;
        }
        return infer(ctx, child, e->b, out, err);
    }
    if (e->kind == E_PAIR)
    {
        st = infer(ctx, env, e->a, &t1, err);
        if (st != HM_OK)
        {
            return st;
        }
        st = infer(ctx, env, e->b, &t2, err);
        if (st != HM_OK)
        {
            return st;
        }
        pair_type = hm_type_app2(ctx, "Pair", t1, t2);
        *out = pair_type;
        return HM_OK;
    }
    return HM_ERROR_INTERNAL;
}

static hm_type *infer_ok(hm_ctx *ctx, hm_env *env, struct expr *e,
                         hm_status *st_out)
{
    hm_error *err;
    hm_type *result;
    hm_status st;
    err = NULL;
    result = NULL;
    st = infer(ctx, env, e, &result, &err);
    if (st_out != NULL)
    {
        *st_out = st;
    }
    return result;
}

static void run_identity(void)
{
    hm_ctx *ctx;
    hm_env *env;
    hm_type *result;
    hm_status st;
    struct expr *x;
    x = enode(E_VAR, "x", NULL, NULL);
    ctx = hm_ctx_new(NULL);
    env = hm_env_new(ctx);
    result = infer_ok(ctx, env, enode(E_LAM, "x", x, NULL), &st);
    CHECK(st == HM_OK);
    CHECK_TYPE(result, "->('a,'a)");
    hm_ctx_destroy(ctx);
}

static void run_constant(void)
{
    hm_ctx *ctx;
    hm_env *env;
    hm_type *result;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    env = hm_env_new(ctx);
    result = infer_ok(
        ctx, env,
        enode(E_LAM, "x",
              enode(E_LAM, "y", enode(E_VAR, "x", NULL, NULL), NULL), NULL),
        &st);
    CHECK(st == HM_OK);
    CHECK_TYPE(result, "->('a,->('b,'a))");
    hm_ctx_destroy(ctx);
}

static void run_apply(void)
{
    hm_ctx *ctx;
    hm_env *env;
    hm_type *result;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    env = hm_env_new(ctx);
    result =
        infer_ok(ctx, env,
                 enode(E_LAM, "f",
                       enode(E_LAM, "x",
                             enode(E_APP, NULL, enode(E_VAR, "f", NULL, NULL),
                                   enode(E_VAR, "x", NULL, NULL)),
                             NULL),
                       NULL),
                 &st);
    CHECK(st == HM_OK);
    CHECK_TYPE(result, "->(->('a,'b),->('a,'b))");
    hm_ctx_destroy(ctx);
}

static void run_let_poly(void)
{
    hm_ctx *ctx;
    hm_env *env;
    hm_type *result;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    env = hm_env_new(ctx);
    result =
        infer_ok(ctx, env,
                 enode(E_LET, "id",
                       enode(E_LAM, "x", enode(E_VAR, "x", NULL, NULL), NULL),
                       enode(E_PAIR, NULL,
                             enode(E_APP, NULL, enode(E_VAR, "id", NULL, NULL),
                                   enode(E_LIT, "Int", NULL, NULL)),
                             enode(E_APP, NULL, enode(E_VAR, "id", NULL, NULL),
                                   enode(E_LIT, "Bool", NULL, NULL)))),
                 &st);
    CHECK(st == HM_OK);
    CHECK_TYPE(result, "Pair(Int,Bool)");
    hm_ctx_destroy(ctx);
}

static void run_self_application_reject(void)
{
    hm_ctx *ctx;
    hm_env *env;
    hm_type *result;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    env = hm_env_new(ctx);
    result = infer_ok(ctx, env,
                      enode(E_LAM, "x",
                            enode(E_APP, NULL, enode(E_VAR, "x", NULL, NULL),
                                  enode(E_VAR, "x", NULL, NULL)),
                            NULL),
                      &st);
    CHECK(st == HM_ERROR_OCCURS);
    CHECK(result == NULL);
    hm_ctx_destroy(ctx);
}

static void run_lambda_monomorphism_reject(void)
{
    hm_ctx *ctx;
    hm_env *env;
    hm_type *result;
    hm_status st;
    struct expr *id_use1;
    struct expr *id_use2;
    struct expr *pair_body;
    struct expr *whole;
    id_use1 = enode(E_APP, NULL, enode(E_VAR, "id", NULL, NULL),
                    enode(E_LIT, "Int", NULL, NULL));
    id_use2 = enode(E_APP, NULL, enode(E_VAR, "id", NULL, NULL),
                    enode(E_LIT, "Bool", NULL, NULL));
    pair_body = enode(E_PAIR, NULL, id_use1, id_use2);
    whole = enode(E_LAM, "id", pair_body, NULL);
    ctx = hm_ctx_new(NULL);
    env = hm_env_new(ctx);
    result = infer_ok(ctx, env, whole, &st);
    CHECK(st == HM_ERROR_MISMATCH);
    CHECK(result == NULL);
    hm_ctx_destroy(ctx);
}

static void run_unbound(void)
{
    hm_ctx *ctx;
    hm_env *env;
    hm_type *result;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    env = hm_env_new(ctx);
    result = infer_ok(ctx, env, enode(E_VAR, "missing", NULL, NULL), &st);
    CHECK(st == HM_ERROR_UNBOUND);
    CHECK(result == NULL);
    hm_ctx_destroy(ctx);
}

int main(void)
{
    run_identity();
    run_constant();
    run_apply();
    run_let_poly();
    run_self_application_reject();
    run_lambda_monomorphism_reject();
    run_unbound();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
