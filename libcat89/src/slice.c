/* cat89_slice.c - the slice category C/A. */

#include "cat89_internal.h"
#include <cat89/slice.h>

struct snode
{
    struct snode *next;
    cat89_mor *f;
};

struct smor
{
    unsigned long refs;
    cat89_category *category;
    const struct snode *dom;
    const struct snode *cod;
    cat89_mor *h;
    struct smor *next_live;
    cat89_allocator allocator;
};

struct sctx
{
    cat89_allocator allocator;
    cat89_category *base;
    cat89_category *category;
    cat89_eq *eq;
    const cat89_obj *apex;
    struct snode *nodes;
    struct smor *live_mors;
};

static const struct snode *node_read(const cat89_obj *obj)
{
    return (const struct snode *)(const void *)obj;
}

static const struct smor *smor_read(const cat89_mor *mor)
{
    return (const struct smor *)(const void *)mor;
}

static struct smor *smor_handle(cat89_mor *mor)
{
    return (struct smor *)mor;
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

static cat89_status snode_matches(struct sctx *sx, const struct snode *o,
                                  cat89_mor *f, int *out_match)
{
    int fe;
    cat89_status st;

    st = cat89_mor_equal(sx->eq, o->f, f, &fe);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_match = fe;
    return CAT89_OK;
}

static cat89_status snode_intern(struct sctx *sx, cat89_mor *f,
                                 struct snode **out)
{
    struct snode *o;
    struct snode *newnode;
    int match;
    cat89_status st;

    for (o = sx->nodes; o != NULL; o = o->next)
    {
        st = snode_matches(sx, o, f, &match);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (match)
        {
            *out = o;
            return CAT89_OK;
        }
    }
    st = cat89_mor_retain(sx->base, f);
    if (st != CAT89_OK)
    {
        return st;
    }
    newnode = cat89_alloc(&sx->allocator, sizeof(*newnode));
    if (newnode == NULL)
    {
        cat89_mor_release(sx->base, f);
        return CAT89_NOMEM;
    }
    newnode->f = f;
    newnode->next = sx->nodes;
    sx->nodes = newnode;
    *out = newnode;
    return CAT89_OK;
}

static cat89_status slice_dom_cb(void *ctx, const cat89_mor *mor,
                                 const cat89_obj **out_obj)
{
    const struct smor *m;

    (void)ctx;
    m = smor_read(mor);
    *out_obj = (const cat89_obj *)m->dom;
    return CAT89_OK;
}

static cat89_status slice_cod_cb(void *ctx, const cat89_mor *mor,
                                 const cat89_obj **out_obj)
{
    const struct smor *m;

    (void)ctx;
    m = smor_read(mor);
    *out_obj = (const cat89_obj *)m->cod;
    return CAT89_OK;
}

static void relcat(struct sctx *sx)
{
    cat89_category_release(sx->category);
}

static void relh_cat(struct sctx *sx, cat89_mor *h)
{
    cat89_mor_release(sx->base, h);
    cat89_category_release(sx->category);
}

static cat89_status smor_assemble(struct sctx *sx, const struct snode *dom,
                                  const struct snode *cod, cat89_mor *h,
                                  cat89_mor **out)
{
    struct smor *m;
    cat89_status st;

    st = cat89_category_retain(sx->category);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_mor_retain(sx->base, h);
    if (st != CAT89_OK)
    {
        relcat(sx);
        return st;
    }
    m = cat89_alloc(&sx->allocator, sizeof(*m));
    if (m == NULL)
    {
        relh_cat(sx, h);
        return CAT89_NOMEM;
    }
    m->refs = 1;
    m->category = sx->category;
    m->dom = dom;
    m->cod = cod;
    m->h = h;
    m->next_live = sx->live_mors;
    sx->live_mors = m;
    m->allocator = sx->allocator;
    *out = (cat89_mor *)m;
    return CAT89_OK;
}

static cat89_status slice_square_ok(struct sctx *sx, const struct snode *dom,
                                    const struct snode *cod, cat89_mor *h,
                                    int *out_ok)
{
    cat89_mor *comp;
    cat89_status st;
    int same;

    comp = NULL;
    st = cat89_compose(sx->base, cod->f, h, &comp);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_mor_equal(sx->eq, comp, dom->f, &same);
    cat89_mor_release(sx->base, comp);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_ok = same;
    return CAT89_OK;
}

static cat89_status slice_identity_cb(void *ctx, const cat89_obj *obj,
                                      cat89_mor **out_mor)
{
    struct sctx *sx = ctx;
    const struct snode *n;
    const cat89_obj *src;
    cat89_mor *ih;
    cat89_mor *mout;
    cat89_status st;

    n = node_read(obj);
    src = NULL;
    st = cat89_dom(sx->base, n->f, &src);
    if (st != CAT89_OK)
    {
        return st;
    }
    ih = NULL;
    mout = NULL;
    *out_mor = NULL;
    st = cat89_identity(sx->base, src, &ih);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = smor_assemble(sx, n, n, ih, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    cat89_mor_release(sx->base, ih);
    return st;
}

static cat89_status slice_compose_cb(void *ctx, const cat89_mor *g,
                                     const cat89_mor *f, cat89_mor **out_mor)
{
    struct sctx *sx = ctx;
    const struct smor *gm;
    const struct smor *fm;
    cat89_mor *nh;
    cat89_mor *mout;
    cat89_status st;

    gm = smor_read(g);
    fm = smor_read(f);
    nh = NULL;
    mout = NULL;
    *out_mor = NULL;
    st = cat89_compose(sx->base, gm->h, fm->h, &nh);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = smor_assemble(sx, fm->dom, gm->cod, nh, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    cat89_mor_release(sx->base, nh);
    return st;
}

static int slice_obj_same_cb(void *ctx, const cat89_obj *a, const cat89_obj *b)
{
    (void)ctx;
    return a == b;
}

static cat89_status slice_retain_cb(void *ctx, cat89_mor *mor)
{
    struct smor *m;
    cat89_status st;

    (void)ctx;
    m = smor_handle(mor);
    st = cat89_ref_inc(&m->refs);
    return st;
}

static int smor_drop(struct smor *m)
{
    int dropped;

    dropped = cat89_ref_dec(&m->refs);
    return dropped;
}

static struct smor **smor_unlink_next(struct smor **link)
{
    return &(*link)->next_live;
}

static void smor_unlink_remove(struct smor **link, struct smor *m)
{
    *link = m->next_live;
}

static void smor_unlink(struct sctx *sx, struct smor *m)
{
    struct smor **link;

    link = &sx->live_mors;
    while (*link != NULL)
    {
        if (*link == m)
        {
            smor_unlink_remove(link, m);
            return;
        }
        link = smor_unlink_next(link);
    }
}

static void smor_dispose(struct sctx *sx, struct smor *m)
{
    smor_unlink(sx, m);
    cat89_mor_release(sx->base, m->h);
    cat89_category_release(m->category);
    cat89_free(&m->allocator, m);
}

static void slice_release_cb(void *ctx, cat89_mor *mor)
{
    struct sctx *sx = ctx;
    struct smor *m;
    int last;

    m = smor_handle(mor);
    last = smor_drop(m);
    if (last)
    {
        smor_dispose(sx, m);
    }
}

static struct snode *node_drop_next(struct sctx *sx, struct snode *o)
{
    struct snode *p;

    p = o->next;
    cat89_mor_release(sx->base, o->f);
    cat89_free(&sx->allocator, o);
    return p;
}

static void sctx_free_nodes(struct sctx *sx)
{
    struct snode *o;

    o = sx->nodes;
    while (o != NULL)
    {
        o = node_drop_next(sx, o);
    }
}

static void slice_destroy_cb(void *ctx)
{
    struct sctx *sx = ctx;

    sctx_free_nodes(sx);
    cat89_eq_release(sx->eq);
    cat89_category_release(sx->base);
    cat89_free(&sx->allocator, sx);
}

static int slice_owns_obj_cb(void *ctx, const cat89_obj *obj)
{
    struct sctx *sx = ctx;
    struct snode *o;

    for (o = sx->nodes; o != NULL; o = o->next)
    {
        if ((const cat89_obj *)(const void *)o == obj)
        {
            return 1;
        }
    }
    return 0;
}

static int slice_owns_mor_cb(void *ctx, const cat89_mor *mor)
{
    struct sctx *sx = ctx;
    struct smor *m;

    for (m = sx->live_mors; m != NULL; m = m->next_live)
    {
        if ((const cat89_mor *)(const void *)m == mor)
        {
            return 1;
        }
    }
    return 0;
}

static const cat89_category_ops slice_ops = {
    slice_dom_cb,      slice_cod_cb,    slice_identity_cb, slice_compose_cb,
    slice_obj_same_cb, slice_retain_cb, slice_release_cb,  slice_owns_obj_cb,
    slice_owns_mor_cb, slice_destroy_cb};

static void slice_abort_eq_cat(cat89_eq *eq, cat89_category *cat)
{
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void slice_abort_full(struct sctx *sx)
{
    cat89_eq_release(sx->eq);
    cat89_category_release(sx->base);
    cat89_free(&sx->allocator, sx);
}

cat89_status cat89_slice_category_new(cat89_category *category, cat89_eq *eq,
                                      const cat89_obj *apex,
                                      const cat89_allocator *allocator,
                                      cat89_category **out_category)
{
    const cat89_allocator *actual;
    struct sctx *sx;
    cat89_status st;
    cat89_status s2;

    if (out_category == NULL)
    {
        return CAT89_INVALID;
    }
    *out_category = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (apex == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    actual = resolve_alloc(allocator);

    st = cat89_category_retain(category);
    if (st != CAT89_OK)
    {
        return st;
    }
    s2 = cat89_eq_retain(eq);
    if (s2 != CAT89_OK)
    {
        cat89_category_release(category);
        return s2;
    }
    sx = cat89_alloc(actual, sizeof(*sx));
    if (sx == NULL)
    {
        slice_abort_eq_cat(eq, category);
        return CAT89_NOMEM;
    }
    sx->allocator = *actual;
    sx->base = category;
    sx->category = NULL;
    sx->eq = eq;
    sx->apex = apex;
    sx->nodes = NULL;
    sx->live_mors = NULL;

    st = cat89_category_new(&slice_ops, sx, actual, &sx->category);
    if (st != CAT89_OK)
    {
        slice_abort_full(sx);
        return st;
    }
    *out_category = sx->category;
    return CAT89_OK;
}

cat89_status cat89_slice_obj_new(cat89_category *slice, cat89_mor *f,
                                 const cat89_obj **out_obj)
{
    struct sctx *sx;
    struct snode *node;
    const cat89_obj *cf;
    int same;
    cat89_status st;

    if (out_obj == NULL)
    {
        return CAT89_INVALID;
    }
    *out_obj = NULL;
    if (slice == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    sx = cat89_category_ctx(slice);
    if (cat89_owns_mor(sx->base, f) == 0)
    {
        return CAT89_INVALID;
    }

    cf = NULL;
    st = cat89_cod(sx->base, f, &cf);
    if (st != CAT89_OK)
    {
        return st;
    }
    same = cat89_obj_same(sx->base, cf, sx->apex);
    if (same == 0)
    {
        return CAT89_DOMAIN;
    }

    node = NULL;
    st = snode_intern(sx, f, &node);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_obj = (const cat89_obj *)node;
    return CAT89_OK;
}

cat89_status cat89_slice_mor_new(cat89_category *slice,
                                 const cat89_obj *dom_obj,
                                 const cat89_obj *cod_obj, cat89_mor *h,
                                 cat89_mor **out_mor)
{
    struct sctx *sx;
    const struct snode *dom;
    const struct snode *cod;
    cat89_mor *mout;
    cat89_status st;
    int ok;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (slice == NULL)
    {
        return CAT89_INVALID;
    }
    if (dom_obj == NULL)
    {
        return CAT89_INVALID;
    }
    if (cod_obj == NULL)
    {
        return CAT89_INVALID;
    }
    if (h == NULL)
    {
        return CAT89_INVALID;
    }
    sx = cat89_category_ctx(slice);
    if (cat89_owns_obj(slice, dom_obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(slice, cod_obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(sx->base, h) == 0)
    {
        return CAT89_INVALID;
    }
    dom = node_read(dom_obj);
    cod = node_read(cod_obj);

    ok = 0;
    st = slice_square_ok(sx, dom, cod, h, &ok);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (ok == 0)
    {
        return CAT89_INVALID;
    }

    mout = NULL;
    st = smor_assemble(sx, dom, cod, h, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    return st;
}

const cat89_mor *cat89_slice_obj_mor(const cat89_category *slice,
                                     const cat89_obj *obj)
{
    const struct snode *n;

    if (slice == NULL)
    {
        return NULL;
    }
    if (obj == NULL)
    {
        return NULL;
    }
    if (cat89_owns_obj(slice, obj) == 0)
    {
        return NULL;
    }
    n = node_read(obj);
    return n->f;
}

const cat89_mor *cat89_slice_mor_mor(const cat89_category *slice,
                                     const cat89_mor *mor)
{
    const struct smor *m;

    if (slice == NULL)
    {
        return NULL;
    }
    if (mor == NULL)
    {
        return NULL;
    }
    if (cat89_owns_mor(slice, mor) == 0)
    {
        return NULL;
    }
    m = smor_read(mor);
    return m->h;
}
