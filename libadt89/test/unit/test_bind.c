/* test_bind.c - binding constructors into an hm_env and instantiation. */
#include "test.h"

static void run_bind_lookup(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    hm_env *env;
    hm_type *a;
    hm_type *fields[1];
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    option = adt_type_new(ctx, "option", 1);
    a = adt_type_parameter(option, 0);
    adt_ctor_add(option, "None", 0, NULL);
    fields[0] = a;
    adt_ctor_add(option, "Some", 1, fields);
    CHECK(adt_type_seal(option) == ADT_OK);
    env = hm_env_new(hm);
    CHECK(adt_type_bind(option, env) == ADT_OK);
    CHECK(hm_env_lookup(env, "None") != NULL);
    CHECK(hm_env_lookup(env, "Some") != NULL);
    CHECK(adt_ctx_bind_all(ctx, env) == ADT_OK);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_bind_unsealed(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    hm_env *env;
    const adt_error *err;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    option = adt_type_new(ctx, "option", 1);
    env = hm_env_new(hm);
    CHECK(adt_type_bind(option, env) == ADT_ERROR_UNSEALED);
    err = adt_ctx_error(ctx);
    CHECK(err != NULL);
    if (err != NULL)
    {
        CHECK(adt_error_kind_of(err) == ADT_ERR_UNSEALED);
    }
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_instantiate_independent(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    adt_ctor *some;
    hm_type *a;
    hm_type *fields[1];
    hm_type *i1;
    hm_type *i2;
    hm_type *int_type;
    hm_type *string_type;
    hm_type *args[1];
    hm_type *opt_int;
    hm_type *opt_string;
    hm_type *f_int;
    hm_type *f_string;
    const hm_scheme *s;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    option = adt_type_new(ctx, "option", 1);
    a = adt_type_parameter(option, 0);
    adt_ctor_add(option, "None", 0, NULL);
    fields[0] = a;
    some = adt_ctor_add(option, "Some", 1, fields);
    CHECK(adt_type_seal(option) == ADT_OK);
    s = adt_ctor_scheme(some);
    i1 = hm_instantiate(hm, s);
    i2 = hm_instantiate(hm, s);
    int_type = hm_type_const(hm, "Int");
    string_type = hm_type_const(hm, "String");
    args[0] = int_type;
    opt_int = adt_type_apply(option, 1, args);
    args[0] = string_type;
    opt_string = adt_type_apply(option, 1, args);
    f_int = hm_type_fun(hm, int_type, opt_int);
    f_string = hm_type_fun(hm, string_type, opt_string);
    CHECK(hm_unify(hm, i1, f_int, NULL) == HM_OK);
    CHECK(hm_unify(hm, i2, f_string, NULL) == HM_OK);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_template_isolation(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    adt_ctor *some;
    hm_type *a;
    hm_type *fields[1];
    hm_type *i1;
    hm_type *i2;
    hm_type *int_type;
    hm_type *args[1];
    hm_type *opt_int;
    hm_type *f_int;
    const hm_scheme *s;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    option = adt_type_new(ctx, "option", 1);
    a = adt_type_parameter(option, 0);
    adt_ctor_add(option, "None", 0, NULL);
    fields[0] = a;
    some = adt_ctor_add(option, "Some", 1, fields);
    CHECK(adt_type_seal(option) == ADT_OK);
    s = adt_ctor_scheme(some);
    i1 = hm_instantiate(hm, s);
    i2 = hm_instantiate(hm, s);
    int_type = hm_type_const(hm, "Int");
    args[0] = int_type;
    opt_int = adt_type_apply(option, 1, args);
    f_int = hm_type_fun(hm, int_type, opt_int);
    CHECK(hm_unify(hm, i1, f_int, NULL) == HM_OK);
    CHECK(hm_unify(hm, i2, f_int, NULL) == HM_OK);
    CHECK(hm_type_kind_of(adt_type_parameter(option, 0)) == HM_TYPE_VAR);
    CHECK(hm_type_prune(adt_type_parameter(option, 0)) ==
          adt_type_parameter(option, 0));
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

int main(void)
{
    run_bind_lookup();
    run_bind_unsealed();
    run_instantiate_independent();
    run_template_isolation();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
