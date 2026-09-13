/* test_pattern.c - constructor-pattern typing constraints. */
#include "test.h"

static void run_some_int(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    adt_ctor *some;
    hm_type *a;
    hm_type *fields[1];
    hm_type *int_type;
    hm_type *args[1];
    hm_type *scrutinee;
    hm_type *out[1];
    hm_error *err;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    option = adt_type_new(ctx, "option", 1);
    a = adt_type_parameter(option, 0);
    adt_ctor_add(option, "None", 0, NULL);
    fields[0] = a;
    some = adt_ctor_add(option, "Some", 1, fields);
    CHECK(adt_type_seal(option) == ADT_OK);
    int_type = hm_type_const(hm, "Int");
    args[0] = int_type;
    scrutinee = adt_type_apply(option, 1, args);
    out[0] = NULL;
    CHECK(adt_pattern_types(ctx, some, scrutinee, 1, out, &err) == ADT_OK);
    CHECK(hm_type_equal(hm_type_prune(out[0]), int_type) == 1);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_wrong_output_count(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    adt_ctor *some;
    hm_type *a;
    hm_type *fields[1];
    hm_type *args[1];
    hm_type *scrutinee;
    const adt_error *err;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    option = adt_type_new(ctx, "option", 1);
    a = adt_type_parameter(option, 0);
    adt_ctor_add(option, "None", 0, NULL);
    fields[0] = a;
    some = adt_ctor_add(option, "Some", 1, fields);
    CHECK(adt_type_seal(option) == ADT_OK);
    args[0] = hm_type_var(hm);
    scrutinee = adt_type_apply(option, 1, args);
    CHECK(adt_pattern_types(ctx, some, scrutinee, 2, NULL, NULL) ==
          ADT_ERROR_PATTERN_ARITY);
    err = adt_ctx_error(ctx);
    CHECK(err != NULL);
    if (err != NULL)
    {
        CHECK(adt_error_kind_of(err) == ADT_ERR_WRONG_PATTERN_ARITY);
    }
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_mismatch_reported(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    adt_type *list;
    adt_ctor *some;
    hm_type *a;
    hm_type *fields[1];
    hm_type *scrut[1];
    hm_type *out[1];
    hm_error *err;
    const adt_error *aerr;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    option = adt_type_new(ctx, "option", 1);
    a = adt_type_parameter(option, 0);
    adt_ctor_add(option, "None", 0, NULL);
    fields[0] = a;
    some = adt_ctor_add(option, "Some", 1, fields);
    CHECK(adt_type_seal(option) == ADT_OK);
    list = adt_type_new(ctx, "list", 1);
    a = adt_type_parameter(list, 0);
    adt_ctor_add(list, "Nil", 0, NULL);
    CHECK(adt_type_seal(list) == ADT_OK);
    scrut[0] = adt_type_parameter(list, 0);
    CHECK(adt_pattern_types(ctx, some, adt_type_apply(list, 1, scrut), 1, out,
                            &err) == ADT_ERROR_TYPE);
    aerr = adt_ctx_error(ctx);
    CHECK(aerr != NULL);
    if (aerr != NULL)
    {
        CHECK(adt_error_kind_of(aerr) == ADT_ERR_HM);
    }
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_null_inputs(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    CHECK(adt_pattern_types(ctx, NULL, NULL, 0, NULL, NULL) ==
          ADT_ERROR_INVALID);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

int main(void)
{
    run_some_int();
    run_wrong_output_count();
    run_mismatch_reported();
    run_null_inputs();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
