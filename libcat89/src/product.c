/* cat89_product.c - the product category C x D. */

#include "cat89_internal.h"
#include <cat89/product.h>

struct pobj
{
    const cat89_obj *l;
    const cat89_obj *r;
    struct pobj *next;
};

struct pmor
{
    unsigned long refs;
    cat89_category *category;
    struct pobj *dom;
    struct pobj *cod;
    cat89_mor *lm;
    cat89_mor *rm;
    struct pmor *next_live;
    cat89_allocator allocator;
};

struct pctx
{
    cat89_allocator allocator;
    cat89_category *left;
    cat89_category *right;
    cat89_category *category;
    struct pobj *objs;
    struct pmor *live_mors;
};

static const struct pobj *obj_read(const cat89_obj *obj)
{
    return (const struct pobj *)(const void *)obj;
}

static const struct pmor *pmor_read(const cat89_mor *mor)
{
    return (const struct pmor *)(const void *)mor;
}

static struct pmor *pmor_handle(cat89_mor *mor)
{
    return (struct pmor *)mor;
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

static int psame_left(struct pctx *pc, const struct pobj *o, const cat89_obj *l)
{
    int result;

    result = cat89_obj_same(pc->left, o->l, l);
    return result;
}

static int psame_right(struct pctx *pc, const struct pobj *o,
                       const cat89_obj *r)
{
    int result;

    result = cat89_obj_same(pc->right, o->r, r);
    return result;
}

static int pair_matches(struct pctx *pc, const struct pobj *o,
                        const cat89_obj *l, const cat89_obj *r)
{
    int s1;
    int s2;

    s1 = psame_left(pc, o, l);
    if (s1 == 0)
    {
        return 0;
    }
    s2 = psame_right(pc, o, r);
    return s2;
}

static cat89_status pobj_intern(struct pctx *pc, const cat89_obj *l,
                                const cat89_obj *r, struct pobj **out)
{
    struct pobj *o;
    struct pobj *newnode;
    int hit;

    for (o = pc->objs; o != NULL; o = o->next)
    {
        hit = pair_matches(pc, o, l, r);
        if (hit)
        {
            *out = o;
            return CAT89_OK;
        }
    }
    newnode = cat89_alloc(&pc->allocator, sizeof(*newnode));
    if (newnode == NULL)
    {
        return CAT89_NOMEM;
    }
    newnode->l = l;
    newnode->r = r;
    newnode->next = pc->objs;
    pc->objs = newnode;
    *out = newnode;
    return CAT89_OK;
}

static void rcat(struct pctx *pc)
{
    cat89_category_release(pc->category);
}

static void rl_cat(struct pctx *pc, cat89_mor *lm)
{
    cat89_mor_release(pc->left, lm);
    cat89_category_release(pc->category);
}

static void prclr(struct pctx *pc, cat89_mor *lm, cat89_mor *rm)
{
    cat89_mor_release(pc->right, rm);
    cat89_mor_release(pc->left, lm);
    cat89_category_release(pc->category);
}

static cat89_status pmor_make(struct pctx *pc, cat89_mor *lm, cat89_mor *rm,
                              cat89_mor **out)
{
    cat89_status st;
    const cat89_obj *dl;
    const cat89_obj *dr;
    const cat89_obj *cl;
    const cat89_obj *cr;
    struct pobj *dtok;
    struct pobj *ctok;
    struct pmor *m;

    dl = NULL;
    dr = NULL;
    cl = NULL;
    cr = NULL;
    dtok = NULL;
    ctok = NULL;
    m = NULL;
    st = cat89_category_retain(pc->category);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_mor_retain(pc->left, lm);
    if (st != CAT89_OK)
    {
        rcat(pc);
        return st;
    }
    st = cat89_mor_retain(pc->right, rm);
    if (st != CAT89_OK)
    {
        rl_cat(pc, lm);
        return st;
    }
    st = cat89_dom(pc->left, lm, &dl);
    if (st != CAT89_OK)
    {
        prclr(pc, lm, rm);
        return st;
    }
    st = cat89_dom(pc->right, rm, &dr);
    if (st != CAT89_OK)
    {
        prclr(pc, lm, rm);
        return st;
    }
    st = cat89_cod(pc->left, lm, &cl);
    if (st != CAT89_OK)
    {
        prclr(pc, lm, rm);
        return st;
    }
    st = cat89_cod(pc->right, rm, &cr);
    if (st != CAT89_OK)
    {
        prclr(pc, lm, rm);
        return st;
    }
    st = pobj_intern(pc, dl, dr, &dtok);
    if (st != CAT89_OK)
    {
        prclr(pc, lm, rm);
        return st;
    }
    st = pobj_intern(pc, cl, cr, &ctok);
    if (st != CAT89_OK)
    {
        prclr(pc, lm, rm);
        return st;
    }
    m = cat89_alloc(&pc->allocator, sizeof(*m));
    if (m == NULL)
    {
        prclr(pc, lm, rm);
        return CAT89_NOMEM;
    }
    m->refs = 1;
    m->category = pc->category;
    m->dom = dtok;
    m->cod = ctok;
    m->lm = lm;
    m->rm = rm;
    m->next_live = pc->live_mors;
    pc->live_mors = m;
    m->allocator = pc->allocator;
    *out = (cat89_mor *)m;
    return CAT89_OK;
}

static cat89_status prod_dom_cb(void *ctx, const cat89_mor *mor,
                                const cat89_obj **out_obj)
{
    const struct pmor *m;

    (void)ctx;
    m = pmor_read(mor);
    *out_obj = (const cat89_obj *)m->dom;
    return CAT89_OK;
}

static cat89_status prod_cod_cb(void *ctx, const cat89_mor *mor,
                                const cat89_obj **out_obj)
{
    const struct pmor *m;

    (void)ctx;
    m = pmor_read(mor);
    *out_obj = (const cat89_obj *)m->cod;
    return CAT89_OK;
}

static cat89_status prod_identity_cb(void *ctx, const cat89_obj *obj,
                                     cat89_mor **out_mor)
{
    struct pctx *pc = ctx;
    const struct pobj *po;
    cat89_mor *idl;
    cat89_mor *idr;
    cat89_mor *mout;
    cat89_status st;

    po = obj_read(obj);
    idl = NULL;
    idr = NULL;
    mout = NULL;
    *out_mor = NULL;
    st = cat89_identity(pc->left, po->l, &idl);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_identity(pc->right, po->r, &idr);
    if (st != CAT89_OK)
    {
        cat89_mor_release(pc->left, idl);
        return st;
    }
    st = pmor_make(pc, idl, idr, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    cat89_mor_release(pc->left, idl);
    cat89_mor_release(pc->right, idr);
    return st;
}

static cat89_status prod_compose_cb(void *ctx, const cat89_mor *g,
                                    const cat89_mor *f, cat89_mor **out_mor)
{
    struct pctx *pc = ctx;
    const struct pmor *gm;
    const struct pmor *fm;
    cat89_mor *lmr;
    cat89_mor *rmr;
    cat89_mor *mout;
    cat89_status st;

    gm = pmor_read(g);
    fm = pmor_read(f);
    lmr = NULL;
    rmr = NULL;
    mout = NULL;
    *out_mor = NULL;
    st = cat89_compose(pc->left, gm->lm, fm->lm, &lmr);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(pc->right, gm->rm, fm->rm, &rmr);
    if (st != CAT89_OK)
    {
        cat89_mor_release(pc->left, lmr);
        return st;
    }
    st = pmor_make(pc, lmr, rmr, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    cat89_mor_release(pc->left, lmr);
    cat89_mor_release(pc->right, rmr);
    return st;
}

static int prod_obj_same_cb(void *ctx, const cat89_obj *a, const cat89_obj *b)
{
    struct pctx *pc = ctx;
    const struct pobj *oa;
    const struct pobj *ob;
    int s1;
    int s2;

    oa = obj_read(a);
    ob = obj_read(b);
    s1 = psame_left(pc, oa, ob->l);
    if (s1 == 0)
    {
        return 0;
    }
    s2 = psame_right(pc, oa, ob->r);
    return s2;
}

static cat89_status prod_retain_cb(void *ctx, cat89_mor *mor)
{
    struct pmor *m;
    cat89_status st;

    (void)ctx;
    m = pmor_handle(mor);
    st = cat89_ref_inc(&m->refs);
    return st;
}

static int pmor_drop(struct pmor *m)
{
    int dropped;

    dropped = cat89_ref_dec(&m->refs);
    return dropped;
}

static struct pmor **pmor_unlink_next(struct pmor **link)
{
    return &(*link)->next_live;
}

static void pmor_unlink_remove(struct pmor **link, struct pmor *m)
{
    *link = m->next_live;
}

static void pmor_unlink(struct pctx *pc, struct pmor *m)
{
    struct pmor **link;

    link = &pc->live_mors;
    while (*link != NULL)
    {
        if (*link == m)
        {
            pmor_unlink_remove(link, m);
            return;
        }
        link = pmor_unlink_next(link);
    }
}

static void pmor_dispose(struct pctx *pc, struct pmor *m)
{
    pmor_unlink(pc, m);
    cat89_mor_release(pc->left, m->lm);
    cat89_mor_release(pc->right, m->rm);
    cat89_category_release(m->category);
    cat89_free(&m->allocator, m);
}

static void prod_release_cb(void *ctx, cat89_mor *mor)
{
    struct pctx *pc = ctx;
    struct pmor *m;
    int last;

    m = pmor_handle(mor);
    last = pmor_drop(m);
    if (last)
    {
        pmor_dispose(pc, m);
    }
}

static struct pobj *token_drop_next(struct pctx *pc, struct pobj *o)
{
    struct pobj *p;

    p = o->next;
    cat89_free(&pc->allocator, o);
    return p;
}

static void pctx_free_objs(struct pctx *pc)
{
    struct pobj *o;

    o = pc->objs;
    while (o != NULL)
    {
        o = token_drop_next(pc, o);
    }
}

static void prod_destroy_cb(void *ctx)
{
    struct pctx *pc = ctx;

    pctx_free_objs(pc);
    cat89_category_release(pc->left);
    cat89_category_release(pc->right);
    cat89_free(&pc->allocator, pc);
}

static int prod_owns_obj_cb(void *ctx, const cat89_obj *obj)
{
    struct pctx *pc = ctx;
    struct pobj *o;

    for (o = pc->objs; o != NULL; o = o->next)
    {
        if ((const cat89_obj *)(const void *)o == obj)
        {
            return 1;
        }
    }
    return 0;
}

static int prod_owns_mor_cb(void *ctx, const cat89_mor *mor)
{
    struct pctx *pc = ctx;
    struct pmor *m;

    for (m = pc->live_mors; m != NULL; m = m->next_live)
    {
        if ((const cat89_mor *)(const void *)m == mor)
        {
            return 1;
        }
    }
    return 0;
}

static const cat89_category_ops prod_ops = {
    prod_dom_cb,      prod_cod_cb,    prod_identity_cb, prod_compose_cb,
    prod_obj_same_cb, prod_retain_cb, prod_release_cb,  prod_owns_obj_cb,
    prod_owns_mor_cb, prod_destroy_cb};

static void prod_new_abort(struct pctx *pc)
{
    cat89_category_release(pc->left);
    cat89_category_release(pc->right);
    cat89_free(&pc->allocator, pc);
}

static void rel_two(cat89_category *a, cat89_category *b)
{
    cat89_category_release(a);
    cat89_category_release(b);
}

cat89_status cat89_product_category_new(cat89_category *left,
                                        cat89_category *right,
                                        const cat89_allocator *allocator,
                                        cat89_category **out_category)
{
    const cat89_allocator *actual;
    struct pctx *pc;
    cat89_status st;

    if (out_category == NULL)
    {
        return CAT89_INVALID;
    }
    *out_category = NULL;
    if (left == NULL)
    {
        return CAT89_INVALID;
    }
    if (right == NULL)
    {
        return CAT89_INVALID;
    }
    actual = resolve_alloc(allocator);

    st = cat89_category_retain(left);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_category_retain(right);
    if (st != CAT89_OK)
    {
        cat89_category_release(left);
        return st;
    }
    pc = cat89_alloc(actual, sizeof(*pc));
    if (pc == NULL)
    {
        rel_two(right, left);
        return CAT89_NOMEM;
    }
    pc->allocator = *actual;
    pc->left = left;
    pc->right = right;
    pc->category = NULL;
    pc->objs = NULL;
    pc->live_mors = NULL;

    st = cat89_category_new(&prod_ops, pc, actual, &pc->category);
    if (st != CAT89_OK)
    {
        prod_new_abort(pc);
        return st;
    }
    *out_category = pc->category;
    return CAT89_OK;
}

cat89_status cat89_product_obj_new(cat89_category *product,
                                   const cat89_obj *left_obj,
                                   const cat89_obj *right_obj,
                                   const cat89_obj **out_obj)
{
    struct pctx *pc;
    struct pobj *tok;
    cat89_status st;

    if (out_obj == NULL)
    {
        return CAT89_INVALID;
    }
    *out_obj = NULL;
    if (product == NULL)
    {
        return CAT89_INVALID;
    }
    if (left_obj == NULL)
    {
        return CAT89_INVALID;
    }
    if (right_obj == NULL)
    {
        return CAT89_INVALID;
    }
    pc = cat89_category_ctx(product);
    if (cat89_owns_obj(pc->left, left_obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(pc->right, right_obj) == 0)
    {
        return CAT89_INVALID;
    }
    tok = NULL;
    st = pobj_intern(pc, left_obj, right_obj, &tok);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_obj = (const cat89_obj *)tok;
    return CAT89_OK;
}

cat89_status cat89_product_mor_new(cat89_category *product, cat89_mor *left_mor,
                                   cat89_mor *right_mor, cat89_mor **out_mor)
{
    struct pctx *pc;
    cat89_mor *mout;
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (product == NULL)
    {
        return CAT89_INVALID;
    }
    if (left_mor == NULL)
    {
        return CAT89_INVALID;
    }
    if (right_mor == NULL)
    {
        return CAT89_INVALID;
    }
    pc = cat89_category_ctx(product);
    if (cat89_owns_mor(pc->left, left_mor) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(pc->right, right_mor) == 0)
    {
        return CAT89_INVALID;
    }
    mout = NULL;
    st = pmor_make(pc, left_mor, right_mor, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    return st;
}

const cat89_obj *cat89_product_left_obj(const cat89_category *product,
                                        const cat89_obj *obj)
{
    const struct pobj *o;

    if (product == NULL)
    {
        return NULL;
    }
    if (obj == NULL)
    {
        return NULL;
    }
    if (cat89_owns_obj(product, obj) == 0)
    {
        return NULL;
    }
    o = obj_read(obj);
    return o->l;
}

const cat89_obj *cat89_product_right_obj(const cat89_category *product,
                                         const cat89_obj *obj)
{
    const struct pobj *o;

    if (product == NULL)
    {
        return NULL;
    }
    if (obj == NULL)
    {
        return NULL;
    }
    if (cat89_owns_obj(product, obj) == 0)
    {
        return NULL;
    }
    o = obj_read(obj);
    return o->r;
}

const cat89_mor *cat89_product_left_mor(const cat89_category *product,
                                        const cat89_mor *mor)
{
    const struct pmor *m;

    if (product == NULL)
    {
        return NULL;
    }
    if (mor == NULL)
    {
        return NULL;
    }
    if (cat89_owns_mor(product, mor) == 0)
    {
        return NULL;
    }
    m = pmor_read(mor);
    return m->lm;
}

const cat89_mor *cat89_product_right_mor(const cat89_category *product,
                                         const cat89_mor *mor)
{
    const struct pmor *m;

    if (product == NULL)
    {
        return NULL;
    }
    if (mor == NULL)
    {
        return NULL;
    }
    if (cat89_owns_mor(product, mor) == 0)
    {
        return NULL;
    }
    m = pmor_read(mor);
    return m->rm;
}
