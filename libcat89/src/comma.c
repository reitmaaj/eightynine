/* cat89_comma.c - the comma category F ↓ G. */

#include "cat89_internal.h"
#include <cat89/comma.h>

struct cnode
{
    struct cnode *next;
    const cat89_obj *c;
    const cat89_obj *d;
    cat89_mor *f;
};

struct cmor
{
    unsigned long refs;
    cat89_category *category;
    const struct cnode *dom;
    const struct cnode *cod;
    cat89_mor *u;
    cat89_mor *v;
    struct cmor *next_live;
    cat89_allocator allocator;
};

struct cctx
{
    cat89_allocator allocator;
    cat89_functor *left;
    cat89_functor *right;
    cat89_eq *eq;
    cat89_category *category;
    cat89_category *ccat;
    cat89_category *dcat;
    cat89_category *ecat;
    struct cnode *nodes;
    struct cmor *live_mors;
};

static const struct cnode *node_read(const cat89_obj *obj)
{
    return (const struct cnode *)(const void *)obj;
}

static const struct cmor *cmor_read(const cat89_mor *mor)
{
    return (const struct cmor *)(const void *)mor;
}

static struct cmor *cmor_handle(cat89_mor *mor)
{
    return (struct cmor *)mor;
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

static int same_c(struct cctx *cx, const struct cnode *o, const cat89_obj *c)
{
    int result;

    result = cat89_obj_same(cx->ccat, o->c, c);
    return result;
}

static int same_d(struct cctx *cx, const struct cnode *o, const cat89_obj *d)
{
    int result;

    result = cat89_obj_same(cx->dcat, o->d, d);
    return result;
}

static cat89_status node_matches(struct cctx *cx, const struct cnode *o,
                                 const cat89_obj *c, cat89_mor *f,
                                 const cat89_obj *d, int *out_match)
{
    int sc;
    int sd;
    int fe;
    cat89_status st;

    sc = same_c(cx, o, c);
    if (sc == 0)
    {
        *out_match = 0;
        return CAT89_OK;
    }
    sd = same_d(cx, o, d);
    if (sd == 0)
    {
        *out_match = 0;
        return CAT89_OK;
    }
    st = cat89_mor_equal(cx->eq, o->f, f, &fe);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_match = fe;
    return CAT89_OK;
}

static cat89_status cnode_intern(struct cctx *cx, const cat89_obj *c,
                                 cat89_mor *f, const cat89_obj *d,
                                 struct cnode **out)
{
    struct cnode *o;
    struct cnode *newnode;
    int match;
    cat89_status st;

    for (o = cx->nodes; o != NULL; o = o->next)
    {
        st = node_matches(cx, o, c, f, d, &match);
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
    st = cat89_mor_retain(cx->ecat, f);
    if (st != CAT89_OK)
    {
        return st;
    }
    newnode = cat89_alloc(&cx->allocator, sizeof(*newnode));
    if (newnode == NULL)
    {
        cat89_mor_release(cx->ecat, f);
        return CAT89_NOMEM;
    }
    newnode->c = c;
    newnode->d = d;
    newnode->f = f;
    newnode->next = cx->nodes;
    cx->nodes = newnode;
    *out = newnode;
    return CAT89_OK;
}

static cat89_status comma_dom_cb(void *ctx, const cat89_mor *mor,
                                 const cat89_obj **out_obj)
{
    const struct cmor *m;

    (void)ctx;
    m = cmor_read(mor);
    *out_obj = (const cat89_obj *)m->dom;
    return CAT89_OK;
}

static cat89_status comma_cod_cb(void *ctx, const cat89_mor *mor,
                                 const cat89_obj **out_obj)
{
    const struct cmor *m;

    (void)ctx;
    m = cmor_read(mor);
    *out_obj = (const cat89_obj *)m->cod;
    return CAT89_OK;
}

static void relcat(struct cctx *cx)
{
    cat89_category_release(cx->category);
}

static void relu_cat(struct cctx *cx, cat89_mor *u)
{
    cat89_mor_release(cx->ccat, u);
    cat89_category_release(cx->category);
}

static void reluv_cat(struct cctx *cx, cat89_mor *u, cat89_mor *v)
{
    cat89_mor_release(cx->dcat, v);
    cat89_mor_release(cx->ccat, u);
    cat89_category_release(cx->category);
}

/* Assemble a comma morphism with the given endpoints and component morphisms,
 * retaining the category and both components. u/v are owned by the caller. */
static cat89_status cmor_assemble(struct cctx *cx, const struct cnode *dom,
                                  const struct cnode *cod, cat89_mor *u,
                                  cat89_mor *v, cat89_mor **out)
{
    struct cmor *m;
    cat89_status st;

    st = cat89_category_retain(cx->category);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_mor_retain(cx->ccat, u);
    if (st != CAT89_OK)
    {
        relcat(cx);
        return st;
    }
    st = cat89_mor_retain(cx->dcat, v);
    if (st != CAT89_OK)
    {
        relu_cat(cx, u);
        return st;
    }
    m = cat89_alloc(&cx->allocator, sizeof(*m));
    if (m == NULL)
    {
        reluv_cat(cx, u, v);
        return CAT89_NOMEM;
    }
    m->refs = 1;
    m->category = cx->category;
    m->dom = dom;
    m->cod = cod;
    m->u = u;
    m->v = v;
    m->next_live = cx->live_mors;
    cx->live_mors = m;
    m->allocator = cx->allocator;
    *out = (cat89_mor *)m;
    return CAT89_OK;
}

static void rel1(cat89_category *ecat, cat89_mor *a)
{
    cat89_mor_release(ecat, a);
}

static void rel2(cat89_category *ecat, cat89_mor *a, cat89_mor *b)
{
    cat89_mor_release(ecat, a);
    cat89_mor_release(ecat, b);
}

static cat89_status comma_endpoints_ok(struct cctx *cx, const struct cnode *dom,
                                       const struct cnode *cod, cat89_mor *u,
                                       cat89_mor *v, int *out_ok)
{
    const cat89_obj *du;
    const cat89_obj *cu;
    const cat89_obj *dv;
    const cat89_obj *cv;
    cat89_status st;
    int s1;
    int s2;
    int s3;
    int s4;

    du = NULL;
    cu = NULL;
    dv = NULL;
    cv = NULL;
    st = cat89_dom(cx->ccat, u, &du);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cod(cx->ccat, u, &cu);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_dom(cx->dcat, v, &dv);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cod(cx->dcat, v, &cv);
    if (st != CAT89_OK)
    {
        return st;
    }
    s1 = cat89_obj_same(cx->ccat, du, dom->c);
    s2 = cat89_obj_same(cx->ccat, cu, cod->c);
    s3 = cat89_obj_same(cx->dcat, dv, dom->d);
    s4 = cat89_obj_same(cx->dcat, cv, cod->d);
    if (s1 == 0)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    if (s2 == 0)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    if (s3 == 0)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    if (s4 == 0)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    return CAT89_OK;
}

static cat89_status comma_square_ok(struct cctx *cx, const struct cnode *dom,
                                    const struct cnode *cod, cat89_mor *u,
                                    cat89_mor *v, int *out_ok)
{
    cat89_category *ecat;
    cat89_mor *fu;
    cat89_mor *gv;
    cat89_mor *lhs;
    cat89_mor *rhs;
    cat89_status st;
    int same;

    ecat = cx->ecat;
    fu = NULL;
    gv = NULL;
    lhs = NULL;
    rhs = NULL;
    st = cat89_functor_map_mor(cx->left, u, &fu);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_map_mor(cx->right, v, &gv);
    if (st != CAT89_OK)
    {
        rel1(ecat, fu);
        return st;
    }
    st = cat89_compose(ecat, gv, dom->f, &lhs);
    rel1(ecat, gv);
    if (st != CAT89_OK)
    {
        rel1(ecat, fu);
        return st;
    }
    st = cat89_compose(ecat, cod->f, fu, &rhs);
    rel1(ecat, fu);
    if (st != CAT89_OK)
    {
        rel1(ecat, lhs);
        return st;
    }
    st = cat89_mor_equal(cx->eq, lhs, rhs, &same);
    rel2(ecat, lhs, rhs);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_ok = same;
    return CAT89_OK;
}

static cat89_status comma_identity_cb(void *ctx, const cat89_obj *obj,
                                      cat89_mor **out_mor)
{
    struct cctx *cx = ctx;
    const struct cnode *n;
    cat89_mor *iu;
    cat89_mor *iv;
    cat89_mor *mout;
    cat89_status st;

    n = node_read(obj);
    iu = NULL;
    iv = NULL;
    mout = NULL;
    *out_mor = NULL;
    st = cat89_identity(cx->ccat, n->c, &iu);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_identity(cx->dcat, n->d, &iv);
    if (st != CAT89_OK)
    {
        cat89_mor_release(cx->ccat, iu);
        return st;
    }
    st = cmor_assemble(cx, n, n, iu, iv, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    cat89_mor_release(cx->ccat, iu);
    cat89_mor_release(cx->dcat, iv);
    return st;
}

static cat89_status comma_compose_cb(void *ctx, const cat89_mor *g,
                                     const cat89_mor *f, cat89_mor **out_mor)
{
    struct cctx *cx = ctx;
    const struct cmor *gm;
    const struct cmor *fm;
    cat89_mor *nu;
    cat89_mor *nv;
    cat89_mor *mout;
    cat89_status st;

    gm = cmor_read(g);
    fm = cmor_read(f);
    nu = NULL;
    nv = NULL;
    mout = NULL;
    *out_mor = NULL;
    st = cat89_compose(cx->ccat, gm->u, fm->u, &nu);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(cx->dcat, gm->v, fm->v, &nv);
    if (st != CAT89_OK)
    {
        cat89_mor_release(cx->ccat, nu);
        return st;
    }
    st = cmor_assemble(cx, fm->dom, gm->cod, nu, nv, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    cat89_mor_release(cx->ccat, nu);
    cat89_mor_release(cx->dcat, nv);
    return st;
}

static int comma_obj_same_cb(void *ctx, const cat89_obj *a, const cat89_obj *b)
{
    (void)ctx;
    return a == b;
}

static cat89_status comma_retain_cb(void *ctx, cat89_mor *mor)
{
    struct cmor *m;
    cat89_status st;

    (void)ctx;
    m = cmor_handle(mor);
    st = cat89_ref_inc(&m->refs);
    return st;
}

static int cmor_drop(struct cmor *m)
{
    int dropped;

    dropped = cat89_ref_dec(&m->refs);
    return dropped;
}

static struct cmor **cmor_unlink_next(struct cmor **link)
{
    return &(*link)->next_live;
}

static void cmor_unlink_remove(struct cmor **link, struct cmor *m)
{
    *link = m->next_live;
}

static void cmor_unlink(struct cctx *cx, struct cmor *m)
{
    struct cmor **link;

    link = &cx->live_mors;
    while (*link != NULL)
    {
        if (*link == m)
        {
            cmor_unlink_remove(link, m);
            return;
        }
        link = cmor_unlink_next(link);
    }
}

static void cmor_dispose(struct cctx *cx, struct cmor *m)
{
    cmor_unlink(cx, m);
    cat89_mor_release(cx->ccat, m->u);
    cat89_mor_release(cx->dcat, m->v);
    cat89_category_release(m->category);
    cat89_free(&m->allocator, m);
}

static void comma_release_cb(void *ctx, cat89_mor *mor)
{
    struct cctx *cx = ctx;
    struct cmor *m;
    int last;

    m = cmor_handle(mor);
    last = cmor_drop(m);
    if (last)
    {
        cmor_dispose(cx, m);
    }
}

static struct cnode *node_drop_next(struct cctx *cx, struct cnode *o)
{
    struct cnode *p;

    p = o->next;
    cat89_mor_release(cx->ecat, o->f);
    cat89_free(&cx->allocator, o);
    return p;
}

static void cctx_free_nodes(struct cctx *cx)
{
    struct cnode *o;

    o = cx->nodes;
    while (o != NULL)
    {
        o = node_drop_next(cx, o);
    }
}

static void comma_destroy_cb(void *ctx)
{
    struct cctx *cx = ctx;

    cctx_free_nodes(cx);
    cat89_functor_release(cx->left);
    cat89_functor_release(cx->right);
    cat89_eq_release(cx->eq);
    cat89_free(&cx->allocator, cx);
}

static void rel_two_functor(cat89_functor *a, cat89_functor *b)
{
    cat89_functor_release(a);
    cat89_functor_release(b);
}

static int comma_owns_obj_cb(void *ctx, const cat89_obj *obj)
{
    struct cctx *cx = ctx;
    struct cnode *o;

    for (o = cx->nodes; o != NULL; o = o->next)
    {
        if ((const cat89_obj *)(const void *)o == obj)
        {
            return 1;
        }
    }
    return 0;
}

static int comma_owns_mor_cb(void *ctx, const cat89_mor *mor)
{
    struct cctx *cx = ctx;
    struct cmor *m;

    for (m = cx->live_mors; m != NULL; m = m->next_live)
    {
        if ((const cat89_mor *)(const void *)m == mor)
        {
            return 1;
        }
    }
    return 0;
}

static const cat89_category_ops comma_ops = {
    comma_dom_cb,      comma_cod_cb,    comma_identity_cb, comma_compose_cb,
    comma_obj_same_cb, comma_retain_cb, comma_release_cb,  comma_owns_obj_cb,
    comma_owns_mor_cb, comma_destroy_cb};

static void comma_new_abort(struct cctx *cx, cat89_functor *left,
                            cat89_functor *right, cat89_eq *eq)
{
    cat89_functor_release(left);
    cat89_functor_release(right);
    cat89_eq_release(eq);
    if (cx != NULL)
    {
        cat89_free(&cx->allocator, cx);
    }
}

cat89_status cat89_comma_category_new(cat89_functor *left, cat89_functor *right,
                                      cat89_eq *target_eq,
                                      const cat89_allocator *allocator,
                                      cat89_category **out_category)
{
    const cat89_allocator *actual;
    struct cctx *cx;
    cat89_category *ecat;
    cat89_category *ecat_r;
    cat89_category *eqcat;
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
    if (target_eq == NULL)
    {
        return CAT89_INVALID;
    }
    ecat = cat89_functor_target(left);
    ecat_r = cat89_functor_target(right);
    if (ecat != ecat_r)
    {
        return CAT89_INVALID;
    }
    eqcat = cat89_eq_category(target_eq);
    if (eqcat != ecat)
    {
        return CAT89_INVALID;
    }
    actual = resolve_alloc(allocator);

    st = cat89_functor_retain(left);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_retain(right);
    if (st != CAT89_OK)
    {
        cat89_functor_release(left);
        return st;
    }
    st = cat89_eq_retain(target_eq);
    if (st != CAT89_OK)
    {
        rel_two_functor(left, right);
        return st;
    }
    cx = cat89_alloc(actual, sizeof(*cx));
    if (cx == NULL)
    {
        comma_new_abort(NULL, left, right, target_eq);
        return CAT89_NOMEM;
    }
    cx->allocator = *actual;
    cx->left = left;
    cx->right = right;
    cx->eq = target_eq;
    cx->category = NULL;
    cx->ccat = cat89_functor_source(left);
    cx->dcat = cat89_functor_source(right);
    cx->ecat = ecat;
    cx->nodes = NULL;
    cx->live_mors = NULL;

    st = cat89_category_new(&comma_ops, cx, actual, &cx->category);
    if (st != CAT89_OK)
    {
        comma_new_abort(cx, left, right, target_eq);
        return st;
    }
    *out_category = cx->category;
    return CAT89_OK;
}

cat89_status cat89_comma_obj_new(cat89_category *comma, const cat89_obj *c,
                                 cat89_mor *f, const cat89_obj *d,
                                 const cat89_obj **out_obj)
{
    struct cctx *cx;
    struct cnode *node;
    const cat89_obj *fc;
    const cat89_obj *gd;
    const cat89_obj *df;
    const cat89_obj *cf;
    cat89_status st;
    int s1;
    int s2;

    if (out_obj == NULL)
    {
        return CAT89_INVALID;
    }
    *out_obj = NULL;
    if (comma == NULL)
    {
        return CAT89_INVALID;
    }
    if (c == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    if (d == NULL)
    {
        return CAT89_INVALID;
    }
    cx = cat89_category_ctx(comma);
    if (cat89_owns_obj(cx->ccat, c) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(cx->dcat, d) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(cx->ecat, f) == 0)
    {
        return CAT89_INVALID;
    }

    fc = NULL;
    gd = NULL;
    df = NULL;
    cf = NULL;
    st = cat89_functor_map_obj(cx->left, c, &fc);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_map_obj(cx->right, d, &gd);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_dom(cx->ecat, f, &df);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cod(cx->ecat, f, &cf);
    if (st != CAT89_OK)
    {
        return st;
    }
    s1 = cat89_obj_same(cx->ecat, df, fc);
    s2 = cat89_obj_same(cx->ecat, cf, gd);
    if (s1 == 0)
    {
        return CAT89_DOMAIN;
    }
    if (s2 == 0)
    {
        return CAT89_DOMAIN;
    }

    node = NULL;
    st = cnode_intern(cx, c, f, d, &node);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_obj = (const cat89_obj *)node;
    return CAT89_OK;
}

cat89_status cat89_comma_mor_new(cat89_category *comma,
                                 const cat89_obj *dom_obj,
                                 const cat89_obj *cod_obj, cat89_mor *u,
                                 cat89_mor *v, cat89_mor **out_mor)
{
    struct cctx *cx;
    const struct cnode *dom;
    const struct cnode *cod;
    cat89_mor *mout;
    cat89_status st;
    int ok;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (comma == NULL)
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
    if (u == NULL)
    {
        return CAT89_INVALID;
    }
    if (v == NULL)
    {
        return CAT89_INVALID;
    }
    cx = cat89_category_ctx(comma);
    if (cat89_owns_obj(comma, dom_obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(comma, cod_obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(cx->ccat, u) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(cx->dcat, v) == 0)
    {
        return CAT89_INVALID;
    }
    dom = node_read(dom_obj);
    cod = node_read(cod_obj);

    ok = 1;
    st = comma_endpoints_ok(cx, dom, cod, u, v, &ok);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (ok == 0)
    {
        return CAT89_DOMAIN;
    }

    ok = 0;
    st = comma_square_ok(cx, dom, cod, u, v, &ok);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (ok == 0)
    {
        return CAT89_INVALID;
    }

    mout = NULL;
    st = cmor_assemble(cx, dom, cod, u, v, &mout);
    if (st == CAT89_OK)
    {
        *out_mor = mout;
    }
    return st;
}

const cat89_obj *cat89_comma_left_obj(const cat89_category *comma,
                                      const cat89_obj *obj)
{
    const struct cnode *n;

    if (comma == NULL)
    {
        return NULL;
    }
    if (obj == NULL)
    {
        return NULL;
    }
    if (cat89_owns_obj(comma, obj) == 0)
    {
        return NULL;
    }
    n = node_read(obj);
    return n->c;
}

const cat89_obj *cat89_comma_right_obj(const cat89_category *comma,
                                       const cat89_obj *obj)
{
    const struct cnode *n;

    if (comma == NULL)
    {
        return NULL;
    }
    if (obj == NULL)
    {
        return NULL;
    }
    if (cat89_owns_obj(comma, obj) == 0)
    {
        return NULL;
    }
    n = node_read(obj);
    return n->d;
}

const cat89_mor *cat89_comma_obj_mor(const cat89_category *comma,
                                     const cat89_obj *obj)
{
    const struct cnode *n;

    if (comma == NULL)
    {
        return NULL;
    }
    if (obj == NULL)
    {
        return NULL;
    }
    if (cat89_owns_obj(comma, obj) == 0)
    {
        return NULL;
    }
    n = node_read(obj);
    return n->f;
}

const cat89_mor *cat89_comma_left_mor(const cat89_category *comma,
                                      const cat89_mor *mor)
{
    const struct cmor *m;

    if (comma == NULL)
    {
        return NULL;
    }
    if (mor == NULL)
    {
        return NULL;
    }
    if (cat89_owns_mor(comma, mor) == 0)
    {
        return NULL;
    }
    m = cmor_read(mor);
    return m->u;
}

const cat89_mor *cat89_comma_right_mor(const cat89_category *comma,
                                       const cat89_mor *mor)
{
    const struct cmor *m;

    if (comma == NULL)
    {
        return NULL;
    }
    if (mor == NULL)
    {
        return NULL;
    }
    if (cat89_owns_mor(comma, mor) == 0)
    {
        return NULL;
    }
    m = cmor_read(mor);
    return m->v;
}
