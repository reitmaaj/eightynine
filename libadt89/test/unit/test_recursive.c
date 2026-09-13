/* test_recursive.c - recursive and mutually recursive datatypes never trigger
 * HM occurs errors through declaration alone. */
#include "test.h"

static void run_recursive_list(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *list;
    adt_ctor *nil;
    adt_ctor *cons;
    hm_type *a;
    hm_type *args[1];
    hm_type *list_a;
    hm_type *fields[2];
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    list = adt_type_new(ctx, "list", 1);
    a = adt_type_parameter(list, 0);
    args[0] = a;
    list_a = adt_type_apply(list, 1, args);
    CHECK(list_a != NULL);
    nil = adt_ctor_add(list, "Nil", 0, NULL);
    fields[0] = a;
    fields[1] = list_a;
    cons = adt_ctor_add(list, "Cons", 2, fields);
    CHECK(nil != NULL);
    CHECK(cons != NULL);
    CHECK(adt_type_seal(list) == ADT_OK);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_mutual_recursion(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *expr;
    adt_type *stmt;
    hm_type *expr_t;
    hm_type *stmt_t;
    adt_ctor *block;
    adt_ctor *exprstmt;
    adt_ctor *ret;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    expr = adt_type_new(ctx, "expr", 0);
    stmt = adt_type_new(ctx, "stmt", 0);
    expr_t = adt_type_apply(expr, 0, NULL);
    stmt_t = adt_type_apply(stmt, 0, NULL);
    CHECK(expr_t != NULL);
    CHECK(stmt_t != NULL);
    ret = adt_ctor_add(expr, "Ret", 0, NULL);
    block = adt_ctor_add(expr, "Block", 1, &stmt_t);
    exprstmt = adt_ctor_add(stmt, "Expr", 1, &expr_t);
    CHECK(ret != NULL);
    CHECK(block != NULL);
    CHECK(exprstmt != NULL);
    CHECK(adt_type_seal(expr) == ADT_OK);
    CHECK(adt_type_seal(stmt) == ADT_OK);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_nested_recursion_no_occurs(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *list;
    adt_ctor *cons;
    hm_type *a;
    hm_type *args[1];
    hm_type *list_a;
    hm_type *fields[2];
    hm_type *out[2];
    hm_type *int_type;
    hm_type *scrut[1];
    hm_error *err;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    list = adt_type_new(ctx, "list", 1);
    a = adt_type_parameter(list, 0);
    args[0] = a;
    list_a = adt_type_apply(list, 1, args);
    adt_ctor_add(list, "Nil", 0, NULL);
    fields[0] = a;
    fields[1] = list_a;
    cons = adt_ctor_add(list, "Cons", 2, fields);
    CHECK(adt_type_seal(list) == ADT_OK);
    int_type = hm_type_const(hm, "Int");
    scrut[0] = int_type;
    CHECK(adt_pattern_types(ctx, cons, adt_type_apply(list, 1, scrut), 2, out,
                            &err) == ADT_OK);
    CHECK(hm_unify(hm, out[0], int_type, NULL) == HM_OK);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

int main(void)
{
    run_recursive_list();
    run_mutual_recursion();
    run_nested_recursion_no_occurs();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
