/* hm_ftv.c - free-variable set routines backing generalization. */
#include <hm.h>

#include "hm_internal.h"

#include <string.h>

struct hm_i_set *hm_i_set_new(hm_ctx *ctx)
{
    struct hm_i_set *set;
    void *raw;
    raw = hm_i_alloc(ctx, sizeof(struct hm_i_set));
    if (raw == NULL)
    {
        return NULL;
    }
    set = raw;
    set->len = 0;
    set->cap = 0;
    set->items = NULL;
    return set;
}

int hm_i_set_has(const struct hm_i_set *set, const struct hm_type *v)
{
    size_t i;
    for (i = 0; i < set->len; ++i)
    {
        if (set->items[i] == v)
        {
            return 1;
        }
    }
    return 0;
}

size_t hm_i_set_count(const struct hm_i_set *set)
{
    return set->len;
}

struct hm_type *hm_i_set_at(const struct hm_i_set *set, size_t i)
{
    return set->items[i];
}

static size_t hm_i_grow_cap(size_t cap)
{
    if (cap == 0)
    {
        return 4;
    }
    return cap * 2;
}

static void hm_i_set_grow(hm_ctx *ctx, struct hm_i_set *set)
{
    size_t newcap;
    size_t bytes;
    size_t copy_bytes;
    void *raw;
    newcap = hm_i_grow_cap(set->cap);
    bytes = newcap * sizeof(struct hm_type *);
    copy_bytes = set->len * sizeof(struct hm_type *);
    raw = hm_i_alloc(ctx, bytes);
    if (raw == NULL)
    {
        return;
    }
    if (set->len != 0)
    {
        memcpy(raw, set->items, copy_bytes);
    }
    set->items = raw;
    set->cap = newcap;
}

void hm_i_set_add(hm_ctx *ctx, struct hm_i_set *set, hm_type *v)
{
    int has;
    has = hm_i_set_has(set, v);
    if (has != 0)
    {
        return;
    }
    if (set->len == set->cap)
    {
        hm_i_set_grow(ctx, set);
    }
    if (set->len == set->cap)
    {
        return;
    }
    set->items[set->len] = v;
    ++set->len;
}

void hm_i_type_ftv(hm_ctx *ctx, hm_type *type, struct hm_i_set *out)
{
    hm_type *p;
    size_t i;
    p = hm_type_prune(type);
    if (p->kind == HM_TYPE_VAR)
    {
        hm_i_set_add(ctx, out, p);
        return;
    }
    for (i = 0; i < p->u.con.arity; ++i)
    {
        hm_i_type_ftv(ctx, p->u.con.args[i], out);
    }
}

int hm_i_var_quantified(const struct hm_type *var,
                        const struct hm_scheme *scheme)
{
    size_t i;
    for (i = 0; i < scheme->count; ++i)
    {
        if (scheme->vars[i] == var)
        {
            return 1;
        }
    }
    return 0;
}

static void hm_i_add_free_index(hm_ctx *ctx, const struct hm_scheme *scheme,
                                struct hm_i_set *body, struct hm_i_set *out,
                                size_t i)
{
    hm_type *item;
    int quant;
    item = hm_i_set_at(body, i);
    quant = hm_i_var_quantified(item, scheme);
    if (quant == 0)
    {
        hm_i_set_add(ctx, out, item);
    }
}

static void hm_i_scheme_ftv(hm_ctx *ctx, const struct hm_scheme *scheme,
                            struct hm_i_set *out)
{
    struct hm_i_set *body;
    size_t n;
    size_t i;
    body = hm_i_set_new(ctx);
    if (body == NULL)
    {
        return;
    }
    hm_i_type_ftv(ctx, scheme->body, body);
    n = hm_i_set_count(body);
    for (i = 0; i < n; ++i)
    {
        hm_i_add_free_index(ctx, scheme, body, out, i);
    }
}

static void hm_i_scope_ftv(hm_ctx *ctx, struct hm_binding *binding,
                           struct hm_i_set *out)
{
    if (binding == NULL)
    {
        return;
    }
    hm_i_scheme_ftv(ctx, binding->scheme, out);
    hm_i_scope_ftv(ctx, binding->next, out);
}

void hm_i_env_ftv(hm_ctx *ctx, hm_env *env, struct hm_i_set *out)
{
    if (env == NULL)
    {
        return;
    }
    hm_i_scope_ftv(ctx, env->bindings, out);
    hm_i_env_ftv(ctx, env->parent, out);
}
