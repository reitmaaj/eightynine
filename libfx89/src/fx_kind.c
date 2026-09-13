/* fx_kind.c - effect-kind and operation registries. */
#include <string.h>

#include "fx_internal.h"

static fx_kindrec *find_kind(const fx_ctx *ctx, fx_kind_id id)
{
    fx_kindrec *it;
    for (it = ctx->kinds; it != NULL; it = it->next)
    {
        if (it->id == id)
        {
            return it;
        }
    }
    return NULL;
}

static int name_taken(fx_kindrec *head, const char *name)
{
    fx_kindrec *it;
    int cmp;
    for (it = head; it != NULL; it = it->next)
    {
        cmp = strcmp(it->name, name);
        if (cmp == 0)
        {
            return 1;
        }
    }
    return 0;
}

fx_status fx_kind_define(fx_ctx *ctx, const char *name, void *userdata,
                         fx_kind_id *out_id)
{
    fx_kindrec *k;
    char *copy;
    int taken;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (name == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out_id == NULL)
    {
        return FX_ERR_INVALID;
    }
    taken = name_taken(ctx->kinds, name);
    if (taken != 0)
    {
        return FX_ERR_DUPLICATE;
    }
    k = (fx_kindrec *)fx__alloc(ctx, sizeof(fx_kindrec));
    if (k == NULL)
    {
        return FX_ERR_NOMEM;
    }
    copy = fx__strdup(ctx, name);
    if (copy == NULL)
    {
        return FX_ERR_NOMEM;
    }
    k->id = ctx->next_kind;
    ctx->next_kind = ctx->next_kind + 1u;
    k->name = copy;
    k->userdata = userdata;
    k->next = NULL;
    if (ctx->kinds_tail == NULL)
    {
        ctx->kinds = k;
    }
    else
    {
        ctx->kinds_tail->next = k;
    }
    ctx->kinds_tail = k;
    *out_id = k->id;
    return FX_OK;
}

static fx_kindrec *kind_find_name(const fx_ctx *ctx, const char *name)
{
    fx_kindrec *it;
    int cmp;
    for (it = ctx->kinds; it != NULL; it = it->next)
    {
        cmp = strcmp(it->name, name);
        if (cmp == 0)
        {
            return it;
        }
    }
    return NULL;
}

fx_status fx_kind_lookup(const fx_ctx *ctx, const char *name,
                         fx_kind_id *out_id)
{
    fx_kindrec *it;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (name == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out_id == NULL)
    {
        return FX_ERR_INVALID;
    }
    it = kind_find_name(ctx, name);
    if (it == NULL)
    {
        return FX_ERR_UNKNOWN;
    }
    *out_id = it->id;
    return FX_OK;
}

const char *fx_kind_name(const fx_ctx *ctx, fx_kind_id id)
{
    fx_kindrec *k;
    if (ctx == NULL)
    {
        return NULL;
    }
    k = find_kind(ctx, id);
    if (k == NULL)
    {
        return NULL;
    }
    return k->name;
}

void *fx_kind_userdata(const fx_ctx *ctx, fx_kind_id id)
{
    fx_kindrec *k;
    if (ctx == NULL)
    {
        return NULL;
    }
    k = find_kind(ctx, id);
    if (k == NULL)
    {
        return NULL;
    }
    return k->userdata;
}

static fx_oprec *find_op(const fx_ctx *ctx, fx_op_id id)
{
    fx_oprec *it;
    for (it = ctx->ops; it != NULL; it = it->next)
    {
        if (it->id == id)
        {
            return it;
        }
    }
    return NULL;
}

static int op_name_taken(fx_oprec *head, fx_kind_id kind, const char *name)
{
    fx_oprec *it;
    int cmp;
    for (it = head; it != NULL; it = it->next)
    {
        if (it->kind == kind)
        {
            cmp = strcmp(it->name, name);
            if (cmp == 0)
            {
                return 1;
            }
        }
    }
    return 0;
}

fx_status fx_op_define(fx_ctx *ctx, fx_kind_id kind, const char *name,
                       void *userdata, fx_op_id *out_id)
{
    fx_kindrec *owner;
    fx_oprec *o;
    char *copy;
    int taken;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (name == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out_id == NULL)
    {
        return FX_ERR_INVALID;
    }
    owner = find_kind(ctx, kind);
    if (owner == NULL)
    {
        return FX_ERR_UNKNOWN;
    }
    taken = op_name_taken(ctx->ops, kind, name);
    if (taken != 0)
    {
        return FX_ERR_DUPLICATE;
    }
    o = (fx_oprec *)fx__alloc(ctx, sizeof(fx_oprec));
    if (o == NULL)
    {
        return FX_ERR_NOMEM;
    }
    copy = fx__strdup(ctx, name);
    if (copy == NULL)
    {
        return FX_ERR_NOMEM;
    }
    o->id = ctx->next_op;
    ctx->next_op = ctx->next_op + 1u;
    o->kind = kind;
    o->name = copy;
    o->userdata = userdata;
    o->next = NULL;
    if (ctx->ops_tail == NULL)
    {
        ctx->ops = o;
    }
    else
    {
        ctx->ops_tail->next = o;
    }
    ctx->ops_tail = o;
    *out_id = o->id;
    return FX_OK;
}

fx_kind_id fx_op_kind(const fx_ctx *ctx, fx_op_id op)
{
    fx_oprec *o;
    if (ctx == NULL)
    {
        return 0u;
    }
    o = find_op(ctx, op);
    if (o == NULL)
    {
        return 0u;
    }
    return o->kind;
}

const char *fx_op_name(const fx_ctx *ctx, fx_op_id op)
{
    fx_oprec *o;
    if (ctx == NULL)
    {
        return NULL;
    }
    o = find_op(ctx, op);
    if (o == NULL)
    {
        return NULL;
    }
    return o->name;
}

void *fx_op_userdata(const fx_ctx *ctx, fx_op_id op)
{
    fx_oprec *o;
    if (ctx == NULL)
    {
        return NULL;
    }
    o = find_op(ctx, op);
    if (o == NULL)
    {
        return NULL;
    }
    return o->userdata;
}
