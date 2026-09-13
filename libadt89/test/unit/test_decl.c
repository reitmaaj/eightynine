/* test_decl.c - declarations, nominal identity, application, constructors. */
#include "test.h"

static void run_nullary(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *voidty;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    voidty = adt_type_new(ctx, "void", 0);
    CHECK(voidty != NULL);
    CHECK(adt_type_parameter_count(voidty) == 0);
    CHECK(strcmp(adt_type_name(voidty), "void") == 0);
    CHECK(adt_type_sealed(voidty) == 0);
    CHECK(adt_type_seal(voidty) == ADT_OK);
    CHECK(adt_type_sealed(voidty) == 1);
    CHECK(adt_type_seal(voidty) == ADT_OK);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_parameters(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    hm_type *p0;
    hm_type *p1;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    option = adt_type_new(ctx, "option", 1);
    CHECK(adt_type_parameter_count(option) == 1);
    p0 = adt_type_parameter(option, 0);
    p1 = adt_type_parameter(option, 1);
    CHECK(p0 != NULL);
    CHECK(p1 == NULL);
    CHECK(adt_type_parameter_count(NULL) == 0);
    CHECK(adt_type_parameter(NULL, 0) == NULL);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_nominal_identity(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *o1;
    adt_type *o2;
    hm_type *a;
    hm_type *args[1];
    hm_type *t1;
    hm_type *t2;
    hm_type *u;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    o1 = adt_type_new(ctx, "option", 1);
    o2 = adt_type_new(ctx, "option", 1);
    a = hm_type_var(hm);
    args[0] = a;
    t1 = adt_type_apply(o1, 1, args);
    t2 = adt_type_apply(o2, 1, args);
    CHECK(t1 != NULL);
    CHECK(t2 != NULL);
    u = adt_type_apply(o1, 1, args);
    CHECK(hm_unify(hm, t1, u, NULL) == HM_OK);
    CHECK(hm_unify(hm, t1, t2, NULL) == HM_ERROR_MISMATCH);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_wrong_apply_arity(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    hm_type *args[2];
    const adt_error *err;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    option = adt_type_new(ctx, "option", 1);
    args[0] = hm_type_var(hm);
    args[1] = hm_type_var(hm);
    CHECK(adt_type_apply(option, 2, args) == NULL);
    err = adt_ctx_error(ctx);
    CHECK(err != NULL);
    if (err != NULL)
    {
        CHECK(adt_error_kind_of(err) == ADT_ERR_WRONG_TYPE_ARITY);
    }
    CHECK(adt_type_apply(option, 0, NULL) == NULL);
    CHECK(adt_type_apply(NULL, 0, NULL) == NULL);
    CHECK(adt_type_apply(option, 1, NULL) == NULL);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_add_after_seal(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *unit;
    const adt_error *err;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    unit = adt_type_new(ctx, "unit", 0);
    CHECK(adt_type_seal(unit) == ADT_OK);
    CHECK(adt_ctor_add(unit, "Unit", 0, NULL) == NULL);
    err = adt_ctx_error(ctx);
    CHECK(err != NULL);
    if (err != NULL)
    {
        CHECK(adt_error_kind_of(err) == ADT_ERR_SEALED);
    }
    CHECK(adt_type_ctor_count(unit) == 0);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_duplicate_ctor(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *a;
    adt_type *b;
    const adt_error *err;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    a = adt_type_new(ctx, "a", 0);
    b = adt_type_new(ctx, "b", 0);
    CHECK(adt_ctor_add(a, "X", 0, NULL) != NULL);
    CHECK(adt_ctor_add(b, "X", 0, NULL) == NULL);
    err = adt_ctx_error(ctx);
    CHECK(err != NULL);
    if (err != NULL)
    {
        CHECK(adt_error_kind_of(err) == ADT_ERR_DUPLICATE_CTOR);
    }
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_ctor_introspection(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *pair;
    adt_ctor *mk;
    adt_ctor *c;
    hm_type *a;
    hm_type *b;
    hm_type *fields[2];
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    pair = adt_type_new(ctx, "pair", 2);
    a = adt_type_parameter(pair, 0);
    b = adt_type_parameter(pair, 1);
    fields[0] = a;
    fields[1] = b;
    mk = adt_ctor_add(pair, "Mk", 2, fields);
    CHECK(mk != NULL);
    CHECK(strcmp(adt_ctor_name(mk), "Mk") == 0);
    CHECK(adt_ctor_owner(mk) == pair);
    CHECK(adt_ctor_field_count(mk) == 2);
    CHECK(adt_ctor_field_type(mk, 0) == a);
    CHECK(adt_ctor_field_type(mk, 1) == b);
    CHECK(adt_ctor_field_type(mk, 2) == NULL);
    CHECK(adt_ctor_name(NULL) != NULL);
    CHECK(adt_ctor_scheme(mk) == NULL);
    CHECK(adt_type_ctor_count(pair) == 1);
    c = adt_type_ctor(pair, 0);
    CHECK(c == mk);
    CHECK(adt_type_ctor(pair, 1) == NULL);
    CHECK(adt_type_ctor(NULL, 0) == NULL);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_sealed_guard(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *o;
    adt_ctor *some;
    hm_type *a;
    hm_type *fields[1];
    const adt_error *err;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    o = adt_type_new(ctx, "option", 1);
    CHECK(adt_type_seal(o) == ADT_OK);
    a = hm_type_var(hm);
    fields[0] = a;
    some = adt_ctor_add(o, "Some", 1, fields);
    CHECK(some == NULL);
    err = adt_ctx_error(ctx);
    CHECK(err != NULL);
    if (err != NULL)
    {
        CHECK(adt_error_kind_of(err) == ADT_ERR_SEALED);
    }
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_foreign_variable_rejection(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *t;
    hm_type *foreign;
    hm_type *fields[1];
    const adt_error *err;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    t = adt_type_new(ctx, "T", 1);
    foreign = hm_type_var(hm);
    fields[0] = foreign;
    CHECK(adt_ctor_add(t, "C", 1, fields) != NULL);
    CHECK(adt_type_seal(t) == ADT_ERROR_TYPE);
    err = adt_ctx_error(ctx);
    CHECK(err != NULL);
    if (err != NULL)
    {
        CHECK(adt_error_kind_of(err) == ADT_ERR_INVALID);
    }
    CHECK(adt_type_sealed(t) == 0);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

int main(void)
{
    run_nullary();
    run_parameters();
    run_nominal_identity();
    run_wrong_apply_arity();
    run_add_after_seal();
    run_duplicate_ctor();
    run_ctor_introspection();
    run_sealed_guard();
    run_foreign_variable_rejection();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
