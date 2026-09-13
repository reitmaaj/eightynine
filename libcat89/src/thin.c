/* cat89_thin.c - thin category derived from a preorder. */

#include "cat89_internal.h"
#include <cat89/thin.h>

struct thin_obj
{
    const void *value;
    struct thin_obj *next;
};

struct thin_mor
{
    unsigned long refs;
    const struct thin_obj *dom;
    const struct thin_obj *cod;
    struct thin_mor *next_live;
};

struct thin_ctx
{
    cat89_allocator allocator;
    cat89_preorder_ops ops;
    void *prov_ctx;
    struct thin_obj *tokens;
    struct thin_mor *live_mors;
    cat89_category *category;
};

static const struct thin_obj *obj_ptr(const cat89_obj *obj)
{
    return (const struct thin_obj *)(const void *)obj;
}

static const void *obj_value(const cat89_obj *obj)
{
    const struct thin_obj *o;

    o = obj_ptr(obj);
    return o->value;
}

static int thin_obj_same_cb(void *ctx, const cat89_obj *a, const cat89_obj *b)
{
    struct thin_ctx *tc = ctx;
    int result;

    result = tc->ops.same(tc->prov_ctx, obj_value(a), obj_value(b));
    return result;
}

static cat89_status thin_dom_cb(void *ctx, const cat89_mor *mor,
                                const cat89_obj **out_obj)
{
    const struct thin_mor *m;

    (void)ctx;
    m = (const struct thin_mor *)(const void *)mor;
    *out_obj = (const cat89_obj *)m->dom;
    return CAT89_OK;
}

static cat89_status thin_cod_cb(void *ctx, const cat89_mor *mor,
                                const cat89_obj **out_obj)
{
    const struct thin_mor *m;

    (void)ctx;
    m = (const struct thin_mor *)(const void *)mor;
    *out_obj = (const cat89_obj *)m->cod;
    return CAT89_OK;
}

static cat89_status mor_make(struct thin_ctx *tc, const struct thin_obj *dom,
                             const struct thin_obj *cod, cat89_mor **out_mor)
{
    struct thin_mor *m;

    m = cat89_alloc(&tc->allocator, sizeof(*m));
    if (m == NULL)
    {
        return CAT89_NOMEM;
    }
    m->refs = 1;
    m->dom = dom;
    m->cod = cod;
    m->next_live = tc->live_mors;
    tc->live_mors = m;
    *out_mor = (cat89_mor *)m;
    return CAT89_OK;
}

static cat89_status thin_identity_cb(void *ctx, const cat89_obj *obj,
                                     cat89_mor **out_mor)
{
    struct thin_ctx *tc = ctx;
    const struct thin_obj *o;
    cat89_status st;

    o = obj_ptr(obj);
    st = mor_make(tc, o, o, out_mor);
    return st;
}

static cat89_status thin_compose_cb(void *ctx, const cat89_mor *g,
                                    const cat89_mor *f, cat89_mor **out_mor)
{
    struct thin_ctx *tc = ctx;
    const struct thin_mor *gm;
    const struct thin_mor *fm;
    cat89_status st;

    gm = (const struct thin_mor *)(const void *)g;
    fm = (const struct thin_mor *)(const void *)f;
    st = mor_make(tc, fm->dom, gm->cod, out_mor);
    return st;
}

static cat89_status thin_retain_cb(void *ctx, cat89_mor *mor)
{
    struct thin_mor *m;
    cat89_status st;

    (void)ctx;
    m = (struct thin_mor *)mor;
    st = cat89_ref_inc(&m->refs);
    return st;
}

static int thin_mor_drop(struct thin_mor *m)
{
    int dropped;

    dropped = cat89_ref_dec(&m->refs);
    return dropped;
}

static struct thin_mor **thin_mor_unlink_next(struct thin_mor **link)
{
    return &(*link)->next_live;
}

static void thin_mor_unlink_remove(struct thin_mor **link, struct thin_mor *m)
{
    *link = m->next_live;
}

static void thin_mor_unlink(struct thin_ctx *tc, struct thin_mor *m)
{
    struct thin_mor **link;

    link = &tc->live_mors;
    while (*link != NULL)
    {
        if (*link == m)
        {
            thin_mor_unlink_remove(link, m);
            return;
        }
        link = thin_mor_unlink_next(link);
    }
}

static void thin_mor_dispose(struct thin_ctx *tc, struct thin_mor *m)
{
    thin_mor_unlink(tc, m);
    cat89_free(&tc->allocator, m);
}

static void thin_release_cb(void *ctx, cat89_mor *mor)
{
    struct thin_ctx *tc = ctx;
    struct thin_mor *m;
    int last;

    m = (struct thin_mor *)mor;
    last = thin_mor_drop(m);
    if (last)
    {
        thin_mor_dispose(tc, m);
    }
}

static struct thin_obj *thin_free_one(struct thin_ctx *tc, struct thin_obj *o)
{
    struct thin_obj *next;

    next = o->next;
    cat89_free(&tc->allocator, o);
    return next;
}

static void thin_destroy_cb(void *ctx)
{
    struct thin_ctx *tc = ctx;
    struct thin_obj *o;

    o = tc->tokens;
    while (o != NULL)
    {
        o = thin_free_one(tc, o);
    }
    cat89_free(&tc->allocator, tc);
}

static int thin_owns_obj_cb(void *ctx, const cat89_obj *obj)
{
    struct thin_ctx *tc = ctx;
    struct thin_obj *o;

    for (o = tc->tokens; o != NULL; o = o->next)
    {
        if ((const cat89_obj *)(const void *)o == obj)
        {
            return 1;
        }
    }
    return 0;
}

static int thin_owns_mor_cb(void *ctx, const cat89_mor *mor)
{
    struct thin_ctx *tc = ctx;
    struct thin_mor *m;

    for (m = tc->live_mors; m != NULL; m = m->next_live)
    {
        if ((const cat89_mor *)(const void *)m == mor)
        {
            return 1;
        }
    }
    return 0;
}

static const cat89_category_ops thin_ops = {
    thin_dom_cb,      thin_cod_cb,    thin_identity_cb, thin_compose_cb,
    thin_obj_same_cb, thin_retain_cb, thin_release_cb,  thin_owns_obj_cb,
    thin_owns_mor_cb, thin_destroy_cb};

static const cat89_allocator *resolve_alloc(const cat89_allocator *allocator)
{
    const cat89_allocator *actual;

    if (allocator == NULL)
    {
        actual = cat89_allocator_default();
    }
    else
    {
        actual = allocator;
    }
    return actual;
}

cat89_status cat89_thin_category_new(const cat89_preorder_ops *ops, void *ctx,
                                     const cat89_allocator *allocator,
                                     cat89_category **out_category)
{
    struct thin_ctx *tc;
    const cat89_allocator *actual;
    cat89_status st;

    if (out_category == NULL)
    {
        return CAT89_INVALID;
    }
    *out_category = NULL;
    if (ops == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops->same == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops->leq == NULL)
    {
        return CAT89_INVALID;
    }

    actual = resolve_alloc(allocator);

    tc = cat89_alloc(actual, sizeof(*tc));
    if (tc == NULL)
    {
        return CAT89_NOMEM;
    }
    tc->allocator = *actual;
    tc->ops = *ops;
    tc->prov_ctx = ctx;
    tc->tokens = NULL;
    tc->live_mors = NULL;
    tc->category = NULL;

    st = cat89_category_new(&thin_ops, tc, actual, &tc->category);
    if (st != CAT89_OK)
    {
        cat89_free(actual, tc);
        return st;
    }
    *out_category = tc->category;
    return CAT89_OK;
}

cat89_status cat89_thin_obj(cat89_category *category, const void *value,
                            const cat89_obj **out_obj)
{
    struct thin_ctx *tc;
    struct thin_obj *o;

    if (out_obj == NULL)
    {
        return CAT89_INVALID;
    }
    *out_obj = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (value == NULL)
    {
        return CAT89_INVALID;
    }
    tc = cat89_category_ctx(category);
    o = cat89_alloc(&tc->allocator, sizeof(*o));
    if (o == NULL)
    {
        return CAT89_NOMEM;
    }
    o->value = value;
    o->next = tc->tokens;
    tc->tokens = o;
    *out_obj = (const cat89_obj *)o;
    return CAT89_OK;
}

cat89_status cat89_thin_mor(cat89_category *category, const cat89_obj *a,
                            const cat89_obj *b, cat89_mor **out_mor)
{
    struct thin_ctx *tc;
    const struct thin_obj *da;
    const struct thin_obj *db;
    cat89_status st;
    int leq;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (a == NULL)
    {
        return CAT89_INVALID;
    }
    if (b == NULL)
    {
        return CAT89_INVALID;
    }
    tc = cat89_category_ctx(category);
    da = obj_ptr(a);
    db = obj_ptr(b);

    leq = 0;
    st = tc->ops.leq(tc->prov_ctx, da->value, db->value, &leq);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (leq == 0)
    {
        return CAT89_NOT_FOUND;
    }
    st = mor_make(tc, da, db, out_mor);
    return st;
}
