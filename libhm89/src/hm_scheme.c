/* hm_scheme.c - type schemes, instantiation, and generalization. */
#include <hm.h>

#include "hm_internal.h"

#include <string.h>

static hm_scheme *hm_i_scheme_alloc(hm_ctx *ctx, size_t count)
{
    hm_scheme *s;
    void *raw;
    raw = hm_i_alloc(ctx, sizeof(hm_scheme));
    if (raw == NULL)
    {
        return NULL;
    }
    s = raw;
    s->count = count;
    s->body = NULL;
    s->vars = NULL;
    if (count != 0)
    {
        raw = hm_i_alloc(ctx, count * sizeof(hm_type *));
        if (raw == NULL)
        {
            return NULL;
        }
        s->vars = raw;
    }
    return s;
}

hm_scheme *hm_scheme_mono(hm_ctx *ctx, hm_type *type)
{
    hm_scheme *s;
    s = hm_i_scheme_alloc(ctx, 0);
    if (s == NULL)
    {
        return NULL;
    }
    s->body = type;
    return s;
}

static long hm_i_quant_index(const hm_scheme *scheme, const hm_type *var)
{
    size_t i;
    for (i = 0; i < scheme->count; ++i)
    {
        if (scheme->vars[i] == var)
        {
            return (long)i;
        }
    }
    return -1;
}

static int hm_i_quant_unbound(hm_type *var)
{
    hm_type *root;
    root = hm_type_prune(var);
    if (root->kind != HM_TYPE_VAR)
    {
        return 0;
    }
    if (root != var)
    {
        return 0;
    }
    return 1;
}

static int hm_i_has_dup(hm_type **quantified, size_t upto)
{
    size_t j;
    for (j = 0; j < upto; ++j)
    {
        if (quantified[j] == quantified[upto])
        {
            return 1;
        }
    }
    return 0;
}

hm_scheme *hm_scheme_new(hm_ctx *ctx, size_t quantified_count,
                         hm_type **quantified, hm_type *body)
{
    hm_scheme *s;
    size_t i;
    size_t bytes;
    int ok;
    int dup;
    if (body == NULL)
    {
        return NULL;
    }
    for (i = 0; i < quantified_count; ++i)
    {
        ok = hm_i_quant_unbound(quantified[i]);
        if (ok == 0)
        {
            return NULL;
        }
    }
    for (i = 0; i < quantified_count; ++i)
    {
        dup = hm_i_has_dup(quantified, i);
        if (dup != 0)
        {
            return NULL;
        }
    }
    s = hm_i_scheme_alloc(ctx, quantified_count);
    if (s == NULL)
    {
        return NULL;
    }
    s->body = body;
    bytes = quantified_count * sizeof(hm_type *);
    if (quantified_count != 0)
    {
        memcpy(s->vars, quantified, bytes);
    }
    return s;
}

size_t hm_scheme_quantified_count(const hm_scheme *scheme)
{
    return scheme->count;
}

hm_type *hm_scheme_body(const hm_scheme *scheme)
{
    return scheme->body;
}

static hm_type *hm_i_clone_type(hm_ctx *ctx, const hm_scheme *scheme,
                                hm_type *node, hm_type **fresh);

static hm_type **hm_i_clone_args(hm_ctx *ctx, const hm_scheme *scheme,
                                 hm_type **src, size_t arity, hm_type **fresh)
{
    size_t bytes;
    size_t i;
    void *raw;
    hm_type **args;
    bytes = arity * sizeof(hm_type *);
    raw = hm_i_alloc(ctx, bytes);
    if (raw == NULL)
    {
        return NULL;
    }
    args = raw;
    for (i = 0; i < arity; ++i)
    {
        args[i] = hm_i_clone_type(ctx, scheme, src[i], fresh);
    }
    for (i = 0; i < arity; ++i)
    {
        if (args[i] == NULL)
        {
            return NULL;
        }
    }
    return args;
}
static hm_type *hm_i_clone_type(hm_ctx *ctx, const hm_scheme *scheme,
                                hm_type *node, hm_type **fresh)
{
    hm_type *p;
    hm_type *t;
    long idx;
    p = hm_type_prune(node);
    if (p->kind == HM_TYPE_VAR)
    {
        idx = hm_i_quant_index(scheme, p);
        if (idx >= 0)
        {
            return fresh[idx];
        }
        return p;
    }
    t = hm_i_type_node(ctx);
    if (t == NULL)
    {
        return NULL;
    }
    t->kind = HM_TYPE_CON;
    t->u.con.name = p->u.con.name;
    t->u.con.arity = p->u.con.arity;
    if (p->u.con.arity != 0)
    {
        t->u.con.args =
            hm_i_clone_args(ctx, scheme, p->u.con.args, p->u.con.arity, fresh);
        if (t->u.con.args == NULL)
        {
            return NULL;
        }
    }
    return t;
}

hm_type *hm_instantiate(hm_ctx *ctx, const hm_scheme *scheme)
{
    size_t n;
    size_t i;
    void *raw;
    hm_type **fresh;
    hm_type *result;
    n = scheme->count;
    if (n == 0)
    {
        return scheme->body;
    }
    raw = hm_i_alloc(ctx, n * sizeof(hm_type *));
    if (raw == NULL)
    {
        return NULL;
    }
    fresh = raw;
    for (i = 0; i < n; ++i)
    {
        fresh[i] = hm_type_var(ctx);
    }
    result = hm_i_clone_type(ctx, scheme, scheme->body, fresh);
    return result;
}

static void hm_i_collect_free(hm_ctx *ctx, const struct hm_i_set *free_type,
                              const struct hm_i_set *free_env,
                              struct hm_i_set *quant, size_t i)
{
    hm_type *item;
    int present;
    item = hm_i_set_at(free_type, i);
    present = hm_i_set_has(free_env, item);
    if (present == 0)
    {
        hm_i_set_add(ctx, quant, item);
    }
}

hm_scheme *hm_generalize(hm_ctx *ctx, hm_env *env, hm_type *type)
{
    struct hm_i_set *free_type;
    struct hm_i_set *free_env;
    struct hm_i_set *quant;
    hm_scheme *s;
    size_t n;
    size_t nq;
    size_t i;
    size_t bytes;
    free_type = hm_i_set_new(ctx);
    if (free_type == NULL)
    {
        return NULL;
    }
    hm_i_type_ftv(ctx, type, free_type);
    free_env = hm_i_set_new(ctx);
    if (free_env == NULL)
    {
        return NULL;
    }
    hm_i_env_ftv(ctx, env, free_env);
    quant = hm_i_set_new(ctx);
    if (quant == NULL)
    {
        return NULL;
    }
    n = hm_i_set_count(free_type);
    for (i = 0; i < n; ++i)
    {
        hm_i_collect_free(ctx, free_type, free_env, quant, i);
    }
    nq = hm_i_set_count(quant);
    s = hm_i_scheme_alloc(ctx, nq);
    if (s == NULL)
    {
        return NULL;
    }
    s->body = type;
    bytes = nq * sizeof(hm_type *);
    if (nq != 0)
    {
        memcpy(s->vars, quant->items, bytes);
    }
    return s;
}
