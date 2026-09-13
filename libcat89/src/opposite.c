/* cat89_opposite.c - the opposite category C^op. */

#include "cat89_internal.h"
#include <cat89/opposite.h>

struct om
{
    unsigned long refs;
    cat89_category *opp;
    cat89_category *base;
    cat89_mor *basemor;
    struct om *next_live;
    cat89_allocator allocator;
};

struct opp_ctx
{
    cat89_allocator allocator;
    cat89_category *base;
    cat89_category *category;
    struct om *live_mors;
};

static const struct om *om_cread(const cat89_mor *mor)
{
    return (const struct om *)(const void *)mor;
}

static struct om *om_handle(cat89_mor *mor)
{
    return (struct om *)mor;
}

static void om_fail(cat89_category *opp, cat89_category *basecat,
                    cat89_mor *basemor)
{
    cat89_mor_release(basecat, basemor);
    cat89_category_release(opp);
}

static cat89_status om_make(struct opp_ctx *oc, cat89_mor *base,
                            cat89_mor **out_mor)
{
    cat89_category *opp;
    struct om *m;
    cat89_status st;

    m = NULL;
    opp = oc->category;
    st = cat89_category_retain(opp);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_mor_retain(oc->base, base);
    if (st != CAT89_OK)
    {
        cat89_category_release(opp);
        return st;
    }
    m = cat89_alloc(&oc->allocator, sizeof(*m));
    if (m == NULL)
    {
        om_fail(opp, oc->base, base);
        return CAT89_NOMEM;
    }
    m->refs = 1;
    m->opp = opp;
    m->base = oc->base;
    m->basemor = base;
    m->next_live = oc->live_mors;
    oc->live_mors = m;
    m->allocator = oc->allocator;
    *out_mor = (cat89_mor *)m;
    return CAT89_OK;
}

static cat89_status opp_dom_cb(void *ctx, const cat89_mor *mor,
                               const cat89_obj **out_obj)
{
    const struct om *m;

    cat89_status st;

    (void)ctx;
    m = om_cread(mor);
    st = cat89_cod(m->base, m->basemor, out_obj);
    return st;
}

static cat89_status opp_cod_cb(void *ctx, const cat89_mor *mor,
                               const cat89_obj **out_obj)
{
    const struct om *m;

    cat89_status st;

    (void)ctx;
    m = om_cread(mor);
    st = cat89_dom(m->base, m->basemor, out_obj);
    return st;
}

static cat89_status opp_identity_cb(void *ctx, const cat89_obj *obj,
                                    cat89_mor **out_mor)
{
    struct opp_ctx *oc = ctx;
    cat89_mor *id;
    cat89_status st;

    id = NULL;
    st = cat89_identity(oc->base, obj, &id);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = om_make(oc, id, out_mor);
    cat89_mor_release(oc->base, id);
    return st;
}

static cat89_status opp_compose_cb(void *ctx, const cat89_mor *g,
                                   const cat89_mor *f, cat89_mor **out_mor)
{
    struct opp_ctx *oc = ctx;
    const struct om *gm;
    const struct om *fm;
    cat89_mor *base_res;
    cat89_status st;

    gm = om_cread(g);
    fm = om_cread(f);
    base_res = NULL;
    st = cat89_compose(oc->base, fm->basemor, gm->basemor, &base_res);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = om_make(oc, base_res, out_mor);
    cat89_mor_release(oc->base, base_res);
    return st;
}

static int opp_obj_same_cb(void *ctx, const cat89_obj *a, const cat89_obj *b)
{
    struct opp_ctx *oc = ctx;
    int result;

    result = cat89_obj_same(oc->base, a, b);
    return result;
}

static cat89_status opp_retain_cb(void *ctx, cat89_mor *mor)
{
    struct om *m;
    cat89_status st;

    (void)ctx;
    m = om_handle(mor);
    st = cat89_ref_inc(&m->refs);
    return st;
}

static int om_drop(struct om *m)
{
    int dropped;

    dropped = cat89_ref_dec(&m->refs);
    return dropped;
}

static struct om **om_unlink_next(struct om **link)
{
    return &(*link)->next_live;
}

static void om_unlink_remove(struct om **link, struct om *m)
{
    *link = m->next_live;
}

static void om_unlink(struct opp_ctx *oc, struct om *m)
{
    struct om **link;

    link = &oc->live_mors;
    while (*link != NULL)
    {
        if (*link == m)
        {
            om_unlink_remove(link, m);
            return;
        }
        link = om_unlink_next(link);
    }
}

static void om_dispose(struct opp_ctx *oc, struct om *m)
{
    om_unlink(oc, m);
    cat89_mor_release(m->base, m->basemor);
    cat89_category_release(m->opp);
    cat89_free(&m->allocator, m);
}

static void opp_release_cb(void *ctx, cat89_mor *mor)
{
    struct opp_ctx *oc = ctx;
    struct om *m;
    int last;

    m = om_handle(mor);
    last = om_drop(m);
    if (last)
    {
        om_dispose(oc, m);
    }
}

static void opp_destroy_cb(void *ctx)
{
    struct opp_ctx *oc = ctx;

    cat89_category_release(oc->base);
    cat89_free(&oc->allocator, oc);
}

static int opp_owns_obj_cb(void *ctx, const cat89_obj *obj)
{
    struct opp_ctx *oc = ctx;

    return cat89_owns_obj(oc->base, obj);
}

static int opp_owns_mor_cb(void *ctx, const cat89_mor *mor)
{
    struct opp_ctx *oc = ctx;
    struct om *m;

    for (m = oc->live_mors; m != NULL; m = m->next_live)
    {
        if ((const cat89_mor *)(const void *)m == mor)
        {
            return 1;
        }
    }
    return 0;
}

static const cat89_category_ops opp_ops = {
    opp_dom_cb,      opp_cod_cb,    opp_identity_cb, opp_compose_cb,
    opp_obj_same_cb, opp_retain_cb, opp_release_cb,  opp_owns_obj_cb,
    opp_owns_mor_cb, opp_destroy_cb};

static void opp_new_abort(struct opp_ctx *oc)
{
    cat89_category_release(oc->base);
    cat89_free(&oc->allocator, oc);
}

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

cat89_status cat89_opposite_new(cat89_category *base,
                                const cat89_allocator *allocator,
                                cat89_category **out_category)
{
    struct opp_ctx *oc;
    const cat89_allocator *actual;
    cat89_status st;

    if (out_category == NULL)
    {
        return CAT89_INVALID;
    }
    *out_category = NULL;
    if (base == NULL)
    {
        return CAT89_INVALID;
    }
    actual = resolve_alloc(allocator);

    st = cat89_category_retain(base);
    if (st != CAT89_OK)
    {
        return st;
    }

    oc = cat89_alloc(actual, sizeof(*oc));
    if (oc == NULL)
    {
        cat89_category_release(base);
        return CAT89_NOMEM;
    }
    oc->allocator = *actual;
    oc->base = base;
    oc->category = NULL;
    oc->live_mors = NULL;

    st = cat89_category_new(&opp_ops, oc, actual, &oc->category);
    if (st != CAT89_OK)
    {
        opp_new_abort(oc);
        return st;
    }
    *out_category = oc->category;
    return CAT89_OK;
}

cat89_status cat89_opposite_mor(cat89_category *opposite, cat89_mor *base_mor,
                                cat89_mor **out_mor)
{
    struct opp_ctx *oc;
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (opposite == NULL)
    {
        return CAT89_INVALID;
    }
    if (base_mor == NULL)
    {
        return CAT89_INVALID;
    }
    oc = cat89_category_ctx(opposite);
    if (cat89_owns_mor(oc->base, base_mor) == 0)
    {
        return CAT89_INVALID;
    }
    st = om_make(oc, base_mor, out_mor);
    return st;
}

const cat89_mor *cat89_opposite_base_mor(const cat89_category *opposite,
                                         const cat89_mor *op_mor)
{
    const struct om *m;

    if (opposite == NULL)
    {
        return NULL;
    }
    if (op_mor == NULL)
    {
        return NULL;
    }
    if (cat89_owns_mor(opposite, op_mor) == 0)
    {
        return NULL;
    }
    m = om_cread(op_mor);
    return m->basemor;
}
