/* smoke.c - end-to-end smoke over libadt89: declare option, add None and
 * Some, seal, bind into an hm_env, look up Some, and type `Some` against the
 * scrutinee option(int), verifying the field unifies with int. */
#include <stdio.h>
#include <stdlib.h>

#include <adt.h>

static int report_failure(const char *what)
{
    fprintf(stderr, "smoke failed: %s\n", what);
    return 1;
}

int main(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    adt_ctor *some;
    hm_env *env;
    hm_type *param;
    hm_type *fields[1];
    hm_type *int_type;
    hm_type *args[1];
    hm_type *scrutinee;
    hm_type *out[1];
    const hm_scheme *scheme;
    hm_error *err;
    adt_status st;
    hm_status hst;
    int ok;
    hm = hm_ctx_new(NULL);
    if (hm == NULL)
    {
        return report_failure("hm_ctx_new");
    }
    ctx = adt_ctx_new(hm);
    if (ctx == NULL)
    {
        hm_ctx_destroy(hm);
        return report_failure("adt_ctx_new");
    }
    option = adt_type_new(ctx, "option", 1);
    if (option == NULL)
    {
        return report_failure("adt_type_new");
    }
    param = adt_type_parameter(option, 0);
    if (param == NULL)
    {
        return report_failure("adt_type_parameter");
    }
    if (adt_ctor_add(option, "None", 0, NULL) == NULL)
    {
        return report_failure("adt_ctor_add None");
    }
    fields[0] = param;
    some = adt_ctor_add(option, "Some", 1, fields);
    if (some == NULL)
    {
        return report_failure("adt_ctor_add Some");
    }
    st = adt_type_seal(option);
    if (st != ADT_OK)
    {
        return report_failure("adt_type_seal");
    }
    scheme = adt_ctor_scheme(some);
    if (scheme == NULL)
    {
        return report_failure("adt_ctor_scheme");
    }
    env = hm_env_new(hm);
    if (env == NULL)
    {
        return report_failure("hm_env_new");
    }
    st = adt_type_bind(option, env);
    if (st != ADT_OK)
    {
        return report_failure("adt_type_bind");
    }
    if (hm_env_lookup(env, "Some") == NULL)
    {
        return report_failure("lookup Some");
    }
    int_type = hm_type_const(hm, "Int");
    if (int_type == NULL)
    {
        return report_failure("hm_type_const Int");
    }
    args[0] = int_type;
    scrutinee = adt_type_apply(option, 1, args);
    if (scrutinee == NULL)
    {
        return report_failure("adt_type_apply");
    }
    out[0] = NULL;
    st = adt_pattern_types(ctx, some, scrutinee, 1, out, &err);
    if (st != ADT_OK)
    {
        return report_failure("adt_pattern_types");
    }
    hst = hm_unify(hm, out[0], int_type, NULL);
    if (hst != HM_OK)
    {
        return report_failure("field not int");
    }
    ok = hm_type_equal(hm_type_prune(out[0]), int_type);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
    if (ok == 0)
    {
        return report_failure("pruned field != int");
    }
    printf("smoke ok\n");
    return 0;
}
