/* adt_pattern.c - pattern objects and constructor-pattern typing. */
#include <adt.h>

#include "adt_internal.h"

#include <string.h>

static int adt_i_is_fun(struct hm_type *node)
{
    struct hm_type *p;
    const char *name;
    hm_type_kind kind;
    size_t arity;
    int cmp;
    p = hm_type_prune(node);
    kind = hm_type_kind_of(p);
    if (kind != HM_TYPE_CON)
    {
        return 0;
    }
    name = hm_type_con_name(p);
    if (name == NULL)
    {
        return 0;
    }
    cmp = strcmp(name, "->");
    if (cmp != 0)
    {
        return 0;
    }
    arity = hm_type_con_arity(p);
    if (arity != 2)
    {
        return 0;
    }
    return 1;
}

static adt_status adt_i_decompose_field(adt_ctx *ctx, struct adt_ctor *ctor,
                                        struct hm_type **cur,
                                        struct hm_type **out_field)
{
    int fun;
    fun = adt_i_is_fun(*cur);
    if (fun == 0)
    {
        adt_i_fail(ctx, ADT_ERROR_TYPE, ADT_ERR_INVALID,
                   "constructor scheme is not a function", ctor->owner, ctor,
                   NULL);
        return ADT_ERROR_TYPE;
    }
    *out_field = hm_type_con_arg(*cur, 0);
    *cur = hm_type_con_arg(*cur, 1);
    return ADT_OK;
}

static adt_status adt_i_decompose(adt_ctx *ctx, struct adt_ctor *ctor,
                                  struct hm_type *instance,
                                  struct hm_type **fields,
                                  struct hm_type **result)
{
    struct hm_type *cur;
    size_t i;
    adt_status st;
    cur = instance;
    for (i = 0; i < ctor->field_count; ++i)
    {
        st = adt_i_decompose_field(ctx, ctor, &cur, &fields[i]);
        if (st != ADT_OK)
        {
            return st;
        }
    }
    *result = cur;
    return ADT_OK;
}

static adt_status adt_i_pattern_check(adt_ctx *ctx, struct adt_ctor *ctor,
                                      hm_type *scrutinee, size_t output_count)
{
    if (ctor == NULL)
    {
        adt_i_fail(ctx, ADT_ERROR_INVALID, ADT_ERR_INVALID,
                   "null constructor or scrutinee", NULL, NULL, NULL);
        return ADT_ERROR_INVALID;
    }
    if (scrutinee == NULL)
    {
        adt_i_fail(ctx, ADT_ERROR_INVALID, ADT_ERR_INVALID,
                   "null constructor or scrutinee", NULL, NULL, NULL);
        return ADT_ERROR_INVALID;
    }
    if (output_count != ctor->field_count)
    {
        adt_i_fail(ctx, ADT_ERROR_PATTERN_ARITY, ADT_ERR_WRONG_PATTERN_ARITY,
                   "wrong constructor-pattern arity", ctor->owner, ctor, NULL);
        return ADT_ERROR_PATTERN_ARITY;
    }
    if (ctor->owner->sealed == 0)
    {
        adt_i_fail(ctx, ADT_ERROR_UNSEALED, ADT_ERR_UNSEALED,
                   "cannot pattern-match an unsealed type", ctor->owner, ctor,
                   NULL);
        return ADT_ERROR_UNSEALED;
    }
    return ADT_OK;
}

static void adt_i_prune_fields(struct adt_ctor *ctor,
                               struct hm_type **out_field_types)
{
    size_t i;
    for (i = 0; i < ctor->field_count; ++i)
    {
        out_field_types[i] = hm_type_prune(out_field_types[i]);
    }
}

static struct adt_ctx *adt_i_pick_ctx(struct adt_ctx *ctx,
                                      struct adt_ctor *ctor)
{
    if (ctx != NULL)
    {
        return ctx;
    }
    if (ctor == NULL)
    {
        return NULL;
    }
    return ctor->owner->ctx;
}

adt_status adt_pattern_types(adt_ctx *ctx, adt_ctor *ctor,
                             hm_type *scrutinee_type, size_t output_count,
                             hm_type **out_field_types, hm_error **out_hm_error)
{
    struct adt_ctx *actx;
    struct hm_type *instance;
    struct hm_type *result;
    hm_status st;
    adt_status ast;
    actx = adt_i_pick_ctx(ctx, ctor);
    if (actx == NULL)
    {
        return ADT_ERROR_INVALID;
    }
    if (out_hm_error != NULL)
    {
        *out_hm_error = NULL;
    }
    ast = adt_i_pattern_check(actx, ctor, scrutinee_type, output_count);
    if (ast != ADT_OK)
    {
        return ast;
    }
    instance = hm_instantiate(actx->hm, ctor->scheme);
    if (instance == NULL)
    {
        adt_i_fail(actx, ADT_ERROR_NOMEM, ADT_ERR_INVALID, "out of memory",
                   ctor->owner, ctor, NULL);
        return ADT_ERROR_NOMEM;
    }
    ast = adt_i_decompose(actx, ctor, instance, out_field_types, &result);
    if (ast != ADT_OK)
    {
        return ast;
    }
    st = hm_unify(actx->hm, result, scrutinee_type, out_hm_error);
    if (st != HM_OK)
    {
        if (out_hm_error != NULL)
        {
            adt_i_fail(actx, ADT_ERROR_TYPE, ADT_ERR_HM,
                       "pattern type mismatch", ctor->owner, ctor,
                       *out_hm_error);
        }
        else
        {
            adt_i_fail(actx, ADT_ERROR_TYPE, ADT_ERR_HM,
                       "pattern type mismatch", ctor->owner, ctor, NULL);
        }
        return ADT_ERROR_TYPE;
    }
    adt_i_prune_fields(ctor, out_field_types);
    return ADT_OK;
}

static void adt_i_wildcard_free(adt_ctx *ctx, struct adt_pattern *pattern)
{
    ctx->alloc.free(ctx->alloc.userdata, pattern);
}

adt_pattern *adt_pattern_wildcard(adt_ctx *ctx)
{
    struct adt_pattern *pattern;
    void *raw;
    int reg;
    if (ctx == NULL)
    {
        return NULL;
    }
    raw = adt_i_alloc(ctx, sizeof(struct adt_pattern));
    if (raw == NULL)
    {
        return NULL;
    }
    pattern = raw;
    pattern->kind = ADT_PATTERN_WILDCARD;
    pattern->origin = ctx->origin;
    reg = adt_i_register_pattern(ctx, pattern);
    if (reg == 0)
    {
        adt_i_wildcard_free(ctx, pattern);
        return NULL;
    }
    return pattern;
}

static void adt_i_copy_arg(struct adt_pattern **args, size_t i,
                           adt_pattern **arguments)
{
    args[i] = arguments[i];
}

static adt_status adt_i_ctor_pattern_args(adt_ctx *ctx, size_t argument_count,
                                          adt_pattern **arguments,
                                          struct adt_pattern ***out)
{
    struct adt_pattern **args;
    size_t i;
    size_t bytes;
    void *raw;
    if (argument_count == 0)
    {
        *out = NULL;
        return ADT_OK;
    }
    bytes = argument_count * sizeof(struct adt_pattern *);
    raw = adt_i_alloc(ctx, bytes);
    if (raw == NULL)
    {
        return ADT_ERROR_NOMEM;
    }
    args = raw;
    for (i = 0; i < argument_count; ++i)
    {
        adt_i_copy_arg(args, i, arguments);
    }
    *out = args;
    return ADT_OK;
}

static void adt_i_ctor_pattern_free(adt_ctx *ctx, struct adt_pattern *pattern)
{
    if (pattern->u.ctor.args != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, pattern->u.ctor.args);
    }
    ctx->alloc.free(ctx->alloc.userdata, pattern);
}

adt_pattern *adt_pattern_ctor(adt_ctx *ctx, adt_ctor *ctor,
                              size_t argument_count, adt_pattern **arguments)
{
    struct adt_pattern *pattern;
    struct adt_pattern **args;
    adt_status st;
    void *raw;
    int reg;
    if (ctx == NULL)
    {
        return NULL;
    }
    if (ctor == NULL)
    {
        return NULL;
    }
    if (ctor->owner->sealed == 0)
    {
        adt_i_fail(ctx, ADT_ERROR_UNSEALED, ADT_ERR_UNSEALED,
                   "cannot pattern-match an unsealed type", ctor->owner, ctor,
                   NULL);
        return NULL;
    }
    if (argument_count != ctor->field_count)
    {
        adt_i_fail(ctx, ADT_ERROR_PATTERN_ARITY, ADT_ERR_WRONG_PATTERN_ARITY,
                   "wrong constructor-pattern arity", ctor->owner, ctor, NULL);
        return NULL;
    }
    if (arguments == NULL)
    {
        if (argument_count != 0)
        {
            adt_i_fail(ctx, ADT_ERROR_INVALID, ADT_ERR_INVALID,
                       "null pattern arguments", ctor->owner, ctor, NULL);
            return NULL;
        }
    }
    raw = adt_i_alloc(ctx, sizeof(struct adt_pattern));
    if (raw == NULL)
    {
        return NULL;
    }
    pattern = raw;
    pattern->kind = ADT_PATTERN_CTOR;
    pattern->u.ctor.ctor = ctor;
    pattern->u.ctor.count = argument_count;
    pattern->origin = ctx->origin;
    st = adt_i_ctor_pattern_args(ctx, argument_count, arguments, &args);
    if (st != ADT_OK)
    {
        ctx->alloc.free(ctx->alloc.userdata, pattern);
        return NULL;
    }
    pattern->u.ctor.args = args;
    reg = adt_i_register_pattern(ctx, pattern);
    if (reg == 0)
    {
        adt_i_ctor_pattern_free(ctx, pattern);
        return NULL;
    }
    return pattern;
}
