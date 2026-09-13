/* test_scheme.c - constructor scheme quantification and body validity. */
#include "test.h"

static void run_option_counts(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    adt_ctor *none;
    adt_ctor *some;
    hm_type *a;
    hm_type *fields[1];
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    option = adt_type_new(ctx, "option", 1);
    a = adt_type_parameter(option, 0);
    none = adt_ctor_add(option, "None", 0, NULL);
    fields[0] = a;
    some = adt_ctor_add(option, "Some", 1, fields);
    CHECK(none != NULL);
    CHECK(some != NULL);
    CHECK(adt_type_seal(option) == ADT_OK);
    CHECK(adt_ctor_scheme(none) != NULL);
    CHECK(adt_ctor_scheme(some) != NULL);
    if (adt_ctor_scheme(none) != NULL)
    {
        CHECK(hm_scheme_quantified_count(adt_ctor_scheme(none)) == 1);
    }
    if (adt_ctor_scheme(some) != NULL)
    {
        CHECK(hm_scheme_quantified_count(adt_ctor_scheme(some)) == 1);
    }
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_quantify_all(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *either;
    adt_ctor *left;
    adt_ctor *right;
    hm_type *a;
    hm_type *b;
    hm_type *fields[1];
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    either = adt_type_new(ctx, "either", 2);
    a = adt_type_parameter(either, 0);
    b = adt_type_parameter(either, 1);
    fields[0] = a;
    left = adt_ctor_add(either, "Left", 1, fields);
    fields[0] = b;
    right = adt_ctor_add(either, "Right", 1, fields);
    CHECK(adt_type_seal(either) == ADT_OK);
    if (adt_ctor_scheme(left) != NULL)
    {
        CHECK(hm_scheme_quantified_count(adt_ctor_scheme(left)) == 2);
    }
    if (adt_ctor_scheme(right) != NULL)
    {
        CHECK(hm_scheme_quantified_count(adt_ctor_scheme(right)) == 2);
    }
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_multifield_counts(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *pair;
    adt_ctor *mk;
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
    CHECK(adt_type_seal(pair) == ADT_OK);
    if (adt_ctor_scheme(mk) != NULL)
    {
        CHECK(hm_scheme_quantified_count(adt_ctor_scheme(mk)) == 2);
    }
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_bool_nullary(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *boolean;
    adt_ctor *f;
    adt_ctor *t;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    boolean = adt_type_new(ctx, "bool", 0);
    f = adt_ctor_add(boolean, "False", 0, NULL);
    t = adt_ctor_add(boolean, "True", 0, NULL);
    CHECK(adt_type_seal(boolean) == ADT_OK);
    if (adt_ctor_scheme(f) != NULL)
    {
        CHECK(hm_scheme_quantified_count(adt_ctor_scheme(f)) == 0);
    }
    if (adt_ctor_scheme(t) != NULL)
    {
        CHECK(hm_scheme_quantified_count(adt_ctor_scheme(t)) == 0);
    }
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

int main(void)
{
    run_option_counts();
    run_quantify_all();
    run_multifield_counts();
    run_bool_nullary();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
