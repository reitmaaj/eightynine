/* cat89_coslice.c - the coslice category A/C. */

#include "cat89_internal.h"
#include <cat89/coslice.h>

struct cnnode
{
    struct cnnode *next;
    cat89_mor *f;
};

struct cnmor
{
    unsigned long refs;
    cat89_category *category;
    const struct cnnode *dom;
    const struct cnnode *cod;
    cat89_mor *k;
    struct cnmor *next_live;
    cat89_allocator allocator;
};

struct cnctx
{
    cat89_allocator allocator;
    cat89_category *base;
    cat89_category *category;
    cat89_eq *eq;
    const cat89_obj *source;
    struct cnnode *nodes;
    struct cnmor *live_mors;
};

static const struct cnnode *node_read(const cat89_obj *obj)
{
    return (const struct cnnode *)(const void *)obj;
}

static const struct cnmor *cnmor_read(const cat89_mor *mor)
{
    return (const struct cnmor *)(const void *)mor;
}

static struct cnmor *cnmor_handle(cat89_mor *mor)
{
    return (struct cnmor *)mor;
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

static cat89_status node_matches(struct cnctx *cx, const struct cnnode *o,
                                 cat89_mor *f, int *out_match)
{
    int fe;
    cat89_status st;

    st = cat89_mor_equal(cx->eq, o->f, f, &fe);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_match = fe;
    return CAT89_OK;
}

static cat89_status node_intern(struct cnctx *cx, cat89_mor *f,
                                struct cnnode **out)
{
    struct cnnode *o;
    struct cnnode *newnode;
    int match;
    cat89_status st;

    for (o = cx->nodes; o != NULL; o = o->next)
    {
        st = node_matches(cx, o, f, &match);
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
    st = cat89_mor_retain(cx->base, f);
    if (st != CAT89_OK)
    {
        return st;
    }
    newnode = cat89_alloc(&cx->allocator, sizeof(*newnode));
    if (newnode == NULL)
    {
        cat89_mor_release(cx->base, f);
        return CAT89_NOMEM;
    }
    newnode->f = f;
    newnode->next = cx->nodes;
    cx->nodes = newnode;
    *out = newnode;
    return CAT89_OK;
}

static cat89_status coslice_dom_cb(void *ctx, const cat89_mor *mor,
                                   const cat89_obj **out_obj)
{
    const struct cnmor *m;

    (void)ctx;
    m = cnmor_read(mor);
    *out_obj = (const cat89_obj *)m->dom;
    return CAT89_OK;
}

static cat89_status coslice_cod_cb(void *ctx, const cat89_mor *mor,
                                   const cat89_obj **out_obj)
{
    const struct cnmor *m;

    (void)ctx;
    m = cnmor_read(mor);
    *out_obj = (const cat89_obj *)m->cod;
    return CAT89_OK;
}

static void relcat(struct cnctx *cx)
{
    cat89_category_release(cx->category);
}

static void relk_cat(struct cnctx *cx, cat89_mor *k)
{
    cat89_mor_release(cx->base, k);
    cat89_category_release(cx->category);
}

static cat89_status cnmor_assemble(struct cnctx *cx, const struct cnnode *dom,
                                   const struct cnnode *cod, cat89_mor *k,
                                   cat89_mor **out)
{
    struct cnmor *m;
    cat89_status st;

    st = cat89_category_retain(cx->category);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_mor_retain(cx->base, k);
    if (st != CAT89_OK)
    {
        relcat(cx);
        return st;
    }
    m = cat89_alloc(&cx->allocator, sizeof(*m));
    if (m == NULL)
    {
        relk_cat(cx, k);
        return CAT89_NOMEM;
    }
    m->refs = 1;
    m->category = cx->category;
    m->dom = dom;
    m->cod = cod;
    m->k = k;
    m->next_live = cx->live_mors;
    cx->live_mors = m;
    m->allocator = cx->allocator;
    *out = (cat89_mor *)m;
    return CAT89_OK;
}

static cat89_status coslice_square_ok(struct cnctx *cx,
                                      const struct cnnode *dom,
                                      const struct cnnode *cod, cat89_mor *k,
                                      int *out_ok)
{
    cat89_mor *comp;
    cat89_status st;
    int same;

    comp = NULL;
    st = cat89_compose(cx->base, k, dom->f, &comp);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_mor_equal(cx->eq, comp, cod->f, &same);
    cat89_mor_release(cx->base, comp);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_ok = same;
    return CAT89_OK;
}

static cat89_status coslice_identity_cb(void *ctx, const cat89_obj *obj,
                                        cat89_mor **out_mor)
{
    struct cnctx *cx = ctx;
    const struct cnnode *n;
    const cat89_obj *cod;
    cat89_mor *ik;
    cat89_mor *mout;
    cat89_status st;

    n = node_read(obj);
    cod = NULL;
    st = cat89_cod(cx->base, n->f, &cod);
    if (st != CAT89_OK)
    {
        return st;
    }
    ik = NULL;
    mout = NULL;
    *out_mor = NULL;
    st = cat89_identity(cx->base, cod, &ik);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cnmor_assemble(cx, n, n, ik, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    cat89_mor_release(cx->base, ik);
    return st;
}

static cat89_status coslice_compose_cb(void *ctx, const cat89_mor *g,
                                       const cat89_mor *f, cat89_mor **out_mor)
{
    struct cnctx *cx = ctx;
    const struct cnmor *gm;
    const struct cnmor *fm;
    cat89_mor *nk;
    cat89_mor *mout;
    cat89_status st;

    gm = cnmor_read(g);
    fm = cnmor_read(f);
    nk = NULL;
    mout = NULL;
    *out_mor = NULL;
    st = cat89_compose(cx->base, gm->k, fm->k, &nk);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cnmor_assemble(cx, fm->dom, gm->cod, nk, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    cat89_mor_release(cx->base, nk);
    return st;
}

static int coslice_obj_same_cb(void *ctx, const cat89_obj *a,
                               const cat89_obj *b)
{
    (void)ctx;
    return a == b;
}

static cat89_status coslice_retain_cb(void *ctx, cat89_mor *mor)
{
    struct cnmor *m;
    cat89_status st;

    (void)ctx;
    m = cnmor_handle(mor);
    st = cat89_ref_inc(&m->refs);
    return st;
}

static int cnmor_drop(struct cnmor *m)
{
    int dropped;

    dropped = cat89_ref_dec(&m->refs);
    return dropped;
}

static struct cnmor **cnmor_unlink_next(struct cnmor **link)
{
    return &(*link)->next_live;
}

static void cnmor_unlink_remove(struct cnmor **link, struct cnmor *m)
{
    *link = m->next_live;
}

static void cnmor_unlink(struct cnctx *cx, struct cnmor *m)
{
    struct cnmor **link;

    link = &cx->live_mors;
    while (*link != NULL)
    {
        if (*link == m)
        {
            cnmor_unlink_remove(link, m);
            return;
        }
        link = cnmor_unlink_next(link);
    }
}

static void cnmor_dispose(struct cnctx *cx, struct cnmor *m)
{
    cnmor_unlink(cx, m);
    cat89_mor_release(cx->base, m->k);
    cat89_category_release(m->category);
    cat89_free(&m->allocator, m);
}

static void coslice_release_cb(void *ctx, cat89_mor *mor)
{
    struct cnctx *cx = ctx;
    struct cnmor *m;
    int last;

    m = cnmor_handle(mor);
    last = cnmor_drop(m);
    if (last)
    {
        cnmor_dispose(cx, m);
    }
}

static struct cnnode *node_drop_next(struct cnctx *cx, struct cnnode *o)
{
    struct cnnode *p;

    p = o->next;
    cat89_mor_release(cx->base, o->f);
    cat89_free(&cx->allocator, o);
    return p;
}

static void cnctx_free_nodes(struct cnctx *cx)
{
    struct cnnode *o;

    o = cx->nodes;
    while (o != NULL)
    {
        o = node_drop_next(cx, o);
    }
}

static void coslice_destroy_cb(void *ctx)
{
    struct cnctx *cx = ctx;

    cnctx_free_nodes(cx);
    cat89_eq_release(cx->eq);
    cat89_category_release(cx->base);
    cat89_free(&cx->allocator, cx);
}

static int coslice_owns_obj_cb(void *ctx, const cat89_obj *obj)
{
    struct cnctx *cx = ctx;
    struct cnnode *o;

    for (o = cx->nodes; o != NULL; o = o->next)
    {
        if ((const cat89_obj *)(const void *)o == obj)
        {
            return 1;
        }
    }
    return 0;
}

static int coslice_owns_mor_cb(void *ctx, const cat89_mor *mor)
{
    struct cnctx *cx = ctx;
    struct cnmor *m;

    for (m = cx->live_mors; m != NULL; m = m->next_live)
    {
        if ((const cat89_mor *)(const void *)m == mor)
        {
            return 1;
        }
    }
    return 0;
}

static const cat89_category_ops coslice_ops = {
    coslice_dom_cb,     coslice_cod_cb,      coslice_identity_cb,
    coslice_compose_cb, coslice_obj_same_cb, coslice_retain_cb,
    coslice_release_cb, coslice_owns_obj_cb, coslice_owns_mor_cb,
    coslice_destroy_cb};

static void coslice_abort_eq_cat(cat89_eq *eq, cat89_category *cat)
{
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void coslice_abort_full(struct cnctx *cx)
{
    cat89_eq_release(cx->eq);
    cat89_category_release(cx->base);
    cat89_free(&cx->allocator, cx);
}

cat89_status cat89_coslice_category_new(cat89_category *category, cat89_eq *eq,
                                        const cat89_obj *source,
                                        const cat89_allocator *allocator,
                                        cat89_category **out_category)
{
    const cat89_allocator *actual;
    struct cnctx *cx;
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
    if (source == NULL)
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
    cx = cat89_alloc(actual, sizeof(*cx));
    if (cx == NULL)
    {
        coslice_abort_eq_cat(eq, category);
        return CAT89_NOMEM;
    }
    cx->allocator = *actual;
    cx->base = category;
    cx->category = NULL;
    cx->eq = eq;
    cx->source = source;
    cx->nodes = NULL;
    cx->live_mors = NULL;

    st = cat89_category_new(&coslice_ops, cx, actual, &cx->category);
    if (st != CAT89_OK)
    {
        coslice_abort_full(cx);
        return st;
    }
    *out_category = cx->category;
    return CAT89_OK;
}

cat89_status cat89_coslice_obj_new(cat89_category *coslice, cat89_mor *f,
                                   const cat89_obj **out_obj)
{
    struct cnctx *cx;
    struct cnnode *node;
    const cat89_obj *df;
    int same;
    cat89_status st;

    if (out_obj == NULL)
    {
        return CAT89_INVALID;
    }
    *out_obj = NULL;
    if (coslice == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    cx = cat89_category_ctx(coslice);
    if (cat89_owns_mor(cx->base, f) == 0)
    {
        return CAT89_INVALID;
    }

    df = NULL;
    st = cat89_dom(cx->base, f, &df);
    if (st != CAT89_OK)
    {
        return st;
    }
    same = cat89_obj_same(cx->base, df, cx->source);
    if (same == 0)
    {
        return CAT89_DOMAIN;
    }

    node = NULL;
    st = node_intern(cx, f, &node);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_obj = (const cat89_obj *)node;
    return CAT89_OK;
}

cat89_status cat89_coslice_mor_new(cat89_category *coslice,
                                   const cat89_obj *dom_obj,
                                   const cat89_obj *cod_obj, cat89_mor *k,
                                   cat89_mor **out_mor)
{
    struct cnctx *cx;
    const struct cnnode *dom;
    const struct cnnode *cod;
    cat89_mor *mout;
    cat89_status st;
    int ok;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (coslice == NULL)
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
    if (k == NULL)
    {
        return CAT89_INVALID;
    }
    cx = cat89_category_ctx(coslice);
    if (cat89_owns_obj(coslice, dom_obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(coslice, cod_obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(cx->base, k) == 0)
    {
        return CAT89_INVALID;
    }
    dom = node_read(dom_obj);
    cod = node_read(cod_obj);

    ok = 0;
    st = coslice_square_ok(cx, dom, cod, k, &ok);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (ok == 0)
    {
        return CAT89_INVALID;
    }

    mout = NULL;
    st = cnmor_assemble(cx, dom, cod, k, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    return st;
}

const cat89_mor *cat89_coslice_obj_mor(const cat89_category *coslice,
                                       const cat89_obj *obj)
{
    const struct cnnode *n;

    if (coslice == NULL)
    {
        return NULL;
    }
    if (obj == NULL)
    {
        return NULL;
    }
    if (cat89_owns_obj(coslice, obj) == 0)
    {
        return NULL;
    }
    n = node_read(obj);
    return n->f;
}

const cat89_mor *cat89_coslice_mor_mor(const cat89_category *coslice,
                                       const cat89_mor *mor)
{
    const struct cnmor *m;

    if (coslice == NULL)
    {
        return NULL;
    }
    if (mor == NULL)
    {
        return NULL;
    }
    if (cat89_owns_mor(coslice, mor) == 0)
    {
        return NULL;
    }
    m = cnmor_read(mor);
    return m->k;
}
