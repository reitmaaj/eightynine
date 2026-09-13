/* hm_type.c - type node creation, inspection, pruning, and equality. */
#include <hm.h>

#include "hm_internal.h"

#include <string.h>

struct hm_type *hm_i_type_node(hm_ctx *ctx)
{
    struct hm_type *t;
    void *raw;
    raw = hm_i_alloc(ctx, sizeof(struct hm_type));
    if (raw == NULL)
    {
        return NULL;
    }
    t = raw;
    return t;
}

hm_type *hm_type_var(hm_ctx *ctx)
{
    hm_type *t;
    unsigned long id;
    t = hm_i_type_node(ctx);
    if (t == NULL)
    {
        return NULL;
    }
    id = ctx->next_var_id;
    ctx->next_var_id = ctx->next_var_id + 1;
    t->kind = HM_TYPE_VAR;
    t->u.var.id = id;
    t->u.var.link = NULL;
    return t;
}

hm_type_kind hm_type_kind_of(hm_type *type)
{
    hm_type *t;
    t = hm_type_prune(type);
    if (t->kind == HM_TYPE_VAR)
    {
        return HM_TYPE_VAR;
    }
    return HM_TYPE_CON;
}

unsigned long hm_type_var_id(hm_type *type)
{
    hm_type *t;
    if (type == NULL)
    {
        return 0;
    }
    t = hm_type_prune(type);
    if (t->kind != HM_TYPE_VAR)
    {
        return 0;
    }
    return t->u.var.id;
}

hm_type *hm_type_prune(hm_type *type)
{
    hm_type *v;
    hm_type *r;
    if (type->kind != HM_TYPE_VAR)
    {
        return type;
    }
    v = type;
    if (v->u.var.link == NULL)
    {
        return v;
    }
    r = hm_type_prune(v->u.var.link);
    v->u.var.link = r;
    return r;
}

static hm_type **hm_i_copy_args(hm_ctx *ctx, hm_type **args, size_t arity)
{
    size_t bytes;
    void *raw;
    hm_type **dst;
    bytes = arity * sizeof(hm_type *);
    raw = hm_i_alloc(ctx, bytes);
    if (raw == NULL)
    {
        return NULL;
    }
    dst = raw;
    memcpy(dst, args, bytes);
    return dst;
}

hm_type *hm_type_con(hm_ctx *ctx, const char *name, size_t arity,
                     hm_type **args)
{
    hm_type *t;
    char *n;
    int has_args;
    has_args = 0;
    if (arity != 0)
    {
        has_args = 1;
    }
    n = hm_i_strdup(ctx, name);
    if (n == NULL)
    {
        return NULL;
    }
    t = hm_i_type_node(ctx);
    if (t == NULL)
    {
        return NULL;
    }
    t->kind = HM_TYPE_CON;
    t->u.con.name = n;
    t->u.con.arity = arity;
    if (has_args != 0)
    {
        t->u.con.args = hm_i_copy_args(ctx, args, arity);
        if (t->u.con.args == NULL)
        {
            return NULL;
        }
    }
    return t;
}

hm_type *hm_type_const(hm_ctx *ctx, const char *name)
{
    hm_type *t;
    t = hm_type_con(ctx, name, 0, NULL);
    return t;
}

hm_type *hm_type_app1(hm_ctx *ctx, const char *name, hm_type *a)
{
    hm_type *args[1];
    hm_type *t;
    args[0] = a;
    t = hm_type_con(ctx, name, 1, args);
    return t;
}

hm_type *hm_type_app2(hm_ctx *ctx, const char *name, hm_type *a, hm_type *b)
{
    hm_type *args[2];
    hm_type *t;
    args[0] = a;
    args[1] = b;
    t = hm_type_con(ctx, name, 2, args);
    return t;
}

hm_type *hm_type_fun(hm_ctx *ctx, hm_type *arg, hm_type *result)
{
    hm_type *t;
    t = hm_type_app2(ctx, "->", arg, result);
    return t;
}

const char *hm_type_con_name(hm_type *type)
{
    hm_type *p;
    p = hm_type_prune(type);
    if (p->kind != HM_TYPE_CON)
    {
        return NULL;
    }
    return p->u.con.name;
}

size_t hm_type_con_arity(hm_type *type)
{
    hm_type *p;
    p = hm_type_prune(type);
    if (p->kind != HM_TYPE_CON)
    {
        return 0;
    }
    return p->u.con.arity;
}

hm_type *hm_type_con_arg(hm_type *type, size_t index)
{
    hm_type *p;
    p = hm_type_prune(type);
    if (p->kind != HM_TYPE_CON)
    {
        return NULL;
    }
    if (index >= p->u.con.arity)
    {
        return NULL;
    }
    return p->u.con.args[index];
}

static int hm_type_args_equal(hm_type **left, hm_type **right, size_t arity)
{
    size_t i;
    int eq;
    for (i = 0; i < arity; ++i)
    {
        eq = hm_type_equal(left[i], right[i]);
        if (eq == 0)
        {
            return 0;
        }
    }
    return 1;
}

int hm_type_equal(hm_type *a, hm_type *b)
{
    hm_type *pa;
    hm_type *pb;
    int cmp;
    int result;
    size_t arity;
    pa = hm_type_prune(a);
    pb = hm_type_prune(b);
    if (pa == pb)
    {
        return 1;
    }
    if (pa->kind != pb->kind)
    {
        return 0;
    }
    if (pa->kind == HM_TYPE_VAR)
    {
        return 0;
    }
    cmp = strcmp(pa->u.con.name, pb->u.con.name);
    if (cmp != 0)
    {
        return 0;
    }
    if (pa->u.con.arity != pb->u.con.arity)
    {
        return 0;
    }
    arity = pa->u.con.arity;
    result = hm_type_args_equal(pa->u.con.args, pb->u.con.args, arity);
    return result;
}
