/* adt_scheme.c - sealing validation and constructor-scheme generation. */
#include <adt.h>

#include "adt_internal.h"

static adt_status adt_i_check_node(adt_type *type, struct hm_type *node);

static adt_status adt_i_find_parameter(const struct adt_type *type,
                                       struct hm_type *var)
{
    size_t i;
    for (i = 0; i < type->parameter_count; ++i)
    {
        if (type->parameters[i] == var)
        {
            return ADT_OK;
        }
    }
    return ADT_ERROR_TYPE;
}

static adt_status adt_i_check_var(adt_type *type, struct hm_type *p)
{
    adt_status st;
    st = adt_i_find_parameter(type, p);
    if (st != ADT_OK)
    {
        adt_i_fail(type->ctx, ADT_ERROR_TYPE, ADT_ERR_INVALID,
                   "field type has a free variable outside the datatype "
                   "parameters",
                   type, NULL, NULL);
        return st;
    }
    return ADT_OK;
}

static adt_status adt_i_check_arg_node(adt_type *type, struct hm_type *p,
                                       size_t i)
{
    struct hm_type *arg;
    adt_status st;
    arg = hm_type_con_arg(p, i);
    st = adt_i_check_node(type, arg);
    return st;
}

static adt_status adt_i_check_con(adt_type *type, struct hm_type *p)
{
    size_t arity;
    size_t i;
    adt_status st;
    arity = hm_type_con_arity(p);
    for (i = 0; i < arity; ++i)
    {
        st = adt_i_check_arg_node(type, p, i);
        if (st != ADT_OK)
        {
            return st;
        }
    }
    return ADT_OK;
}

static adt_status adt_i_check_node(adt_type *type, struct hm_type *node)
{
    struct hm_type *p;
    hm_type_kind kind;
    adt_status st;
    p = hm_type_prune(node);
    kind = hm_type_kind_of(p);
    if (kind != HM_TYPE_VAR)
    {
        st = adt_i_check_con(type, p);
        return st;
    }
    st = adt_i_check_var(type, p);
    return st;
}

static adt_status adt_i_check_ctor(adt_type *type, struct adt_ctor *ctor)
{
    size_t i;
    adt_status st;
    for (i = 0; i < ctor->field_count; ++i)
    {
        st = adt_i_check_node(type, ctor->field_types[i]);
        if (st != ADT_OK)
        {
            return st;
        }
    }
    return ADT_OK;
}

static adt_status adt_i_validate(adt_type *type)
{
    size_t i;
    adt_status st;
    for (i = 0; i < type->ctor_count; ++i)
    {
        st = adt_i_check_ctor(type, type->ctors[i]);
        if (st != ADT_OK)
        {
            return st;
        }
    }
    return ADT_OK;
}

static struct hm_type *adt_i_make_result(adt_type *type)
{
    struct hm_type *result;
    result = adt_type_apply(type, type->parameter_count, type->parameters);
    return result;
}

static struct hm_type *adt_i_fold_field(adt_ctx *ctx, struct adt_ctor *ctor,
                                        size_t index, struct hm_type *body)
{
    struct hm_type *t;
    t = hm_type_fun(ctx->hm, ctor->field_types[index], body);
    return t;
}

static struct hm_type *adt_i_make_body(struct hm_type *result,
                                       struct adt_ctor *ctor)
{
    struct hm_type *body;
    size_t i;
    body = result;
    for (i = ctor->field_count; i != 0; --i)
    {
        body = adt_i_fold_field(ctor->owner->ctx, ctor, i - 1, body);
    }
    return body;
}

static adt_status adt_i_gen_one(adt_type *type, struct adt_ctor *ctor,
                                struct hm_type *result)
{
    struct hm_type *body;
    struct hm_scheme *scheme;
    body = adt_i_make_body(result, ctor);
    if (body == NULL)
    {
        adt_i_fail(type->ctx, ADT_ERROR_NOMEM, ADT_ERR_INVALID, "out of memory",
                   type, ctor, NULL);
        return ADT_ERROR_NOMEM;
    }
    scheme = hm_scheme_new(type->ctx->hm, type->parameter_count,
                           type->parameters, body);
    if (scheme == NULL)
    {
        adt_i_fail(type->ctx, ADT_ERROR_NOMEM, ADT_ERR_INVALID,
                   "scheme generation failed", type, ctor, NULL);
        return ADT_ERROR_NOMEM;
    }
    ctor->scheme = scheme;
    return ADT_OK;
}

static adt_status adt_i_generate(adt_type *type, struct hm_type *result)
{
    size_t i;
    adt_status st;
    for (i = 0; i < type->ctor_count; ++i)
    {
        st = adt_i_gen_one(type, type->ctors[i], result);
        if (st != ADT_OK)
        {
            return st;
        }
    }
    return ADT_OK;
}

adt_status adt_type_seal(adt_type *type)
{
    struct hm_type *result;
    adt_status st;
    if (type == NULL)
    {
        return ADT_ERROR_INVALID;
    }
    if (type->sealed != 0)
    {
        return ADT_OK;
    }
    st = adt_i_validate(type);
    if (st != ADT_OK)
    {
        return st;
    }
    result = adt_i_make_result(type);
    if (result == NULL)
    {
        return ADT_ERROR_NOMEM;
    }
    st = adt_i_generate(type, result);
    if (st != ADT_OK)
    {
        return st;
    }
    type->sealed = 1;
    return ADT_OK;
}

const hm_scheme *adt_ctor_scheme(const adt_ctor *ctor)
{
    if (ctor == NULL)
    {
        return NULL;
    }
    return ctor->scheme;
}
