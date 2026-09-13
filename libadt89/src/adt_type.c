/* adt_type.c - declarations, nominal identity, parameters, and application. */
#include <adt.h>

#include "adt_internal.h"

#include <string.h>

static unsigned long adt_i_next_type_id(void)
{
    static unsigned long next_id;
    next_id = next_id + 1;
    return next_id;
}

static void adt_i_append_ulong(char *buf, size_t *pos, unsigned long v)
{
    if (v >= 10)
    {
        adt_i_append_ulong(buf, pos, v / 10);
    }
    buf[*pos] = (char)('0' + (int)(v % 10));
    ++*pos;
}

static char *adt_i_make_hm_name(adt_ctx *ctx, unsigned long id)
{
    char buf[64];
    size_t pos;
    char *name;
    size_t prefix;
    prefix = sizeof(ADT_HM_NAME_PREFIX) - 1;
    memcpy(buf, ADT_HM_NAME_PREFIX, prefix);
    pos = prefix;
    adt_i_append_ulong(buf, &pos, id);
    buf[pos] = '\0';
    name = adt_i_strdup(ctx, buf);
    return name;
}

static adt_status adt_i_type_fill_param(struct adt_type *type, size_t index)
{
    struct hm_type *p;
    p = hm_type_var(type->ctx->hm);
    if (p == NULL)
    {
        return ADT_ERROR_NOMEM;
    }
    type->parameters[index] = p;
    return ADT_OK;
}

static adt_status adt_i_type_alloc_params(adt_ctx *ctx, struct adt_type *type)
{
    size_t i;
    size_t bytes;
    adt_status st;
    void *raw;
    if (type->parameter_count == 0)
    {
        return ADT_OK;
    }
    bytes = type->parameter_count * sizeof(struct hm_type *);
    raw = adt_i_alloc(ctx, bytes);
    if (raw == NULL)
    {
        return ADT_ERROR_NOMEM;
    }
    type->parameters = raw;
    for (i = 0; i < type->parameter_count; ++i)
    {
        st = adt_i_type_fill_param(type, i);
        if (st != ADT_OK)
        {
            return st;
        }
    }
    return ADT_OK;
}

static void adt_i_oom_none(adt_ctx *ctx)
{
    adt_i_fail(ctx, ADT_ERROR_NOMEM, ADT_ERR_INVALID, "out of memory", NULL,
               NULL, NULL);
}

static void adt_i_oom_strings(adt_ctx *ctx, char *n, char *hm_name)
{
    if (n != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, n);
    }
    if (hm_name != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, hm_name);
    }
    adt_i_oom_none(ctx);
}

static void adt_i_oom_type(adt_ctx *ctx, struct adt_type *type)
{
    adt_i_destroy_type(ctx, type);
    adt_i_oom_none(ctx);
}

adt_type *adt_type_new(adt_ctx *ctx, const char *name, size_t parameter_count)
{
    struct adt_type *type;
    char *n;
    char *hm_name;
    unsigned long id;
    adt_status st;
    void *raw;
    int reg;
    if (ctx == NULL)
    {
        return NULL;
    }
    if (name == NULL)
    {
        return NULL;
    }
    id = adt_i_next_type_id();
    n = adt_i_strdup(ctx, name);
    if (n == NULL)
    {
        adt_i_oom_none(ctx);
        return NULL;
    }
    hm_name = adt_i_make_hm_name(ctx, id);
    if (hm_name == NULL)
    {
        adt_i_oom_strings(ctx, n, NULL);
        return NULL;
    }
    raw = adt_i_alloc(ctx, sizeof(struct adt_type));
    if (raw == NULL)
    {
        adt_i_oom_strings(ctx, n, hm_name);
        return NULL;
    }
    type = raw;
    type->ctx = ctx;
    type->name = n;
    type->hm_name = hm_name;
    type->parameter_count = parameter_count;
    type->origin = ctx->origin;
    st = adt_i_type_alloc_params(ctx, type);
    if (st != ADT_OK)
    {
        adt_i_oom_type(ctx, type);
        return NULL;
    }
    reg = adt_i_register_type(ctx, type);
    if (reg == 0)
    {
        adt_i_oom_type(ctx, type);
        return NULL;
    }
    return type;
}

const char *adt_type_name(const adt_type *type)
{
    if (type == NULL)
    {
        return "";
    }
    return type->name;
}

size_t adt_type_parameter_count(const adt_type *type)
{
    if (type == NULL)
    {
        return 0;
    }
    return type->parameter_count;
}

hm_type *adt_type_parameter(const adt_type *type, size_t index)
{
    if (type == NULL)
    {
        return NULL;
    }
    if (index >= type->parameter_count)
    {
        return NULL;
    }
    return type->parameters[index];
}

static adt_status adt_i_args_arity(adt_ctx *ctx, struct adt_type *type,
                                   size_t argument_count)
{
    if (argument_count == type->parameter_count)
    {
        return ADT_OK;
    }
    adt_i_fail(ctx, ADT_ERROR_WRONG_ARITY, ADT_ERR_WRONG_TYPE_ARITY,
               "wrong type-application arity", type, NULL, NULL);
    return ADT_ERROR_WRONG_ARITY;
}

static adt_status adt_i_args_present(adt_ctx *ctx, struct adt_type *type,
                                     size_t argument_count,
                                     struct hm_type **arguments)
{
    if (arguments != NULL)
    {
        return ADT_OK;
    }
    if (argument_count == 0)
    {
        return ADT_OK;
    }
    adt_i_fail(ctx, ADT_ERROR_INVALID, ADT_ERR_INVALID, "null arguments", type,
               NULL, NULL);
    return ADT_ERROR_INVALID;
}

static adt_status adt_i_args_elements(adt_ctx *ctx, struct adt_type *type,
                                      size_t argument_count,
                                      struct hm_type **arguments)
{
    size_t i;
    for (i = 0; i < argument_count; ++i)
    {
        if (arguments[i] == NULL)
        {
            adt_i_fail(ctx, ADT_ERROR_INVALID, ADT_ERR_INVALID, "null argument",
                       type, NULL, NULL);
            return ADT_ERROR_INVALID;
        }
    }
    return ADT_OK;
}

static adt_status adt_i_check_args(adt_ctx *ctx, struct adt_type *type,
                                   size_t argument_count,
                                   struct hm_type **arguments)
{
    adt_status st;
    st = adt_i_args_arity(ctx, type, argument_count);
    if (st != ADT_OK)
    {
        return st;
    }
    st = adt_i_args_present(ctx, type, argument_count, arguments);
    if (st != ADT_OK)
    {
        return st;
    }
    st = adt_i_args_elements(ctx, type, argument_count, arguments);
    if (st != ADT_OK)
    {
        return st;
    }
    return ADT_OK;
}

hm_type *adt_type_apply(adt_type *type, size_t argument_count,
                        hm_type **arguments)
{
    hm_type *t;
    adt_status st;
    if (type == NULL)
    {
        return NULL;
    }
    st = adt_i_check_args(type->ctx, type, argument_count, arguments);
    if (st != ADT_OK)
    {
        return NULL;
    }
    t = hm_type_con(type->ctx->hm, type->hm_name, argument_count, arguments);
    return t;
}

int adt_type_sealed(const adt_type *type)
{
    if (type == NULL)
    {
        return 0;
    }
    return type->sealed;
}

size_t adt_type_ctor_count(const adt_type *type)
{
    if (type == NULL)
    {
        return 0;
    }
    return type->ctor_count;
}

adt_ctor *adt_type_ctor(const adt_type *type, size_t index)
{
    if (type == NULL)
    {
        return NULL;
    }
    if (index >= type->ctor_count)
    {
        return NULL;
    }
    return type->ctors[index];
}
