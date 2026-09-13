/* adt_bind.c - installing constructors into an hm_env. */
#include <adt.h>

#include "adt_internal.h"

static adt_status adt_i_translate(hm_status st)
{
    if (st == HM_OK)
    {
        return ADT_OK;
    }
    if (st == HM_ERROR_NOMEM)
    {
        return ADT_ERROR_NOMEM;
    }
    return ADT_ERROR_INVALID;
}

static adt_status adt_i_bind_fail(adt_type *type, struct adt_ctor *ctor,
                                  hm_status st)
{
    adt_status ast;
    ast = adt_i_translate(st);
    adt_i_fail(type->ctx, ast, ADT_ERR_INVALID, "environment bind failed", type,
               ctor, NULL);
    return ast;
}

static adt_status adt_i_bind_one(adt_type *type, struct adt_ctor *ctor,
                                 hm_env *env)
{
    hm_status st;
    adt_status ast;
    st = hm_env_bind(env, ctor->name, ctor->scheme);
    if (st != HM_OK)
    {
        ast = adt_i_bind_fail(type, ctor, st);
        return ast;
    }
    return ADT_OK;
}

adt_status adt_type_bind(adt_type *type, hm_env *env)
{
    size_t i;
    adt_status st;
    if (type == NULL)
    {
        return ADT_ERROR_INVALID;
    }
    if (env == NULL)
    {
        adt_i_fail(type->ctx, ADT_ERROR_INVALID, ADT_ERR_INVALID,
                   "null environment", type, NULL, NULL);
        return ADT_ERROR_INVALID;
    }
    if (type->sealed == 0)
    {
        adt_i_fail(type->ctx, ADT_ERROR_UNSEALED, ADT_ERR_UNSEALED,
                   "cannot bind an unsealed type", type, NULL, NULL);
        return ADT_ERROR_UNSEALED;
    }
    for (i = 0; i < type->ctor_count; ++i)
    {
        st = adt_i_bind_one(type, type->ctors[i], env);
        if (st != ADT_OK)
        {
            return st;
        }
    }
    return ADT_OK;
}

adt_status adt_ctx_bind_all(adt_ctx *ctx, hm_env *env)
{
    size_t i;
    adt_status st;
    if (ctx == NULL)
    {
        return ADT_ERROR_INVALID;
    }
    for (i = 0; i < ctx->type_count; ++i)
    {
        st = adt_type_bind(ctx->types[i], env);
        if (st != ADT_OK)
        {
            return st;
        }
    }
    return ADT_OK;
}
