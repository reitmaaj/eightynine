/* cat89_nat.c - natural transformations between functors. */

#include "cat89_internal.h"
#include <cat89/nat.h>

struct cat89_nat
{
    unsigned long refs;
    cat89_functor *source;
    cat89_functor *target;
    cat89_nat_ops ops;
    void *ctx;
    cat89_allocator allocator;
};

static void nat_init(cat89_nat *nat, cat89_functor *source,
                     cat89_functor *target, const cat89_nat_ops *ops, void *ctx,
                     const cat89_allocator *allocator)
{
    nat->refs = 1;
    nat->source = source;
    nat->target = target;
    nat->ops = *ops;
    nat->ctx = ctx;
    nat->allocator = *allocator;
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

static void release_functors(cat89_functor *a, cat89_functor *b)
{
    cat89_functor_release(a);
    cat89_functor_release(b);
}

cat89_status cat89_nat_new(cat89_functor *source_functor,
                           cat89_functor *target_functor,
                           const cat89_nat_ops *ops, void *ctx,
                           const cat89_allocator *allocator,
                           cat89_nat **out_nat)
{
    cat89_nat *nat;
    cat89_category *s1;
    cat89_category *s2;
    cat89_category *t1;
    cat89_category *t2;
    const cat89_allocator *actual;
    cat89_status st;

    if (out_nat == NULL)
    {
        return CAT89_INVALID;
    }
    *out_nat = NULL;
    if (source_functor == NULL)
    {
        return CAT89_INVALID;
    }
    if (target_functor == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops->component == NULL)
    {
        return CAT89_INVALID;
    }

    s1 = cat89_functor_source(source_functor);
    s2 = cat89_functor_source(target_functor);
    t1 = cat89_functor_target(source_functor);
    t2 = cat89_functor_target(target_functor);
    if (s1 != s2)
    {
        return CAT89_INVALID;
    }
    if (t1 != t2)
    {
        return CAT89_INVALID;
    }

    actual = resolve_alloc(allocator);

    st = cat89_functor_retain(source_functor);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_retain(target_functor);
    if (st != CAT89_OK)
    {
        cat89_functor_release(source_functor);
        return st;
    }

    nat = cat89_alloc(actual, sizeof(*nat));
    if (nat == NULL)
    {
        release_functors(source_functor, target_functor);
        return CAT89_NOMEM;
    }

    nat_init(nat, source_functor, target_functor, ops, ctx, actual);
    *out_nat = nat;
    return CAT89_OK;
}

static void nat_destroy(cat89_nat *nat)
{
    if (nat->ops.destroy != NULL)
    {
        nat->ops.destroy(nat->ctx);
    }
    cat89_functor_release(nat->source);
    cat89_functor_release(nat->target);
    cat89_free(&nat->allocator, nat);
}

cat89_status cat89_nat_retain(cat89_nat *nat)
{
    cat89_status st;

    if (nat == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&nat->refs);
    return st;
}

void cat89_nat_release(cat89_nat *nat)
{
    int zero;

    if (nat == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&nat->refs);
    if (zero)
    {
        nat_destroy(nat);
    }
}

cat89_functor *cat89_nat_source(const cat89_nat *nat)
{
    if (nat == NULL)
    {
        return NULL;
    }
    return nat->source;
}

cat89_functor *cat89_nat_target(const cat89_nat *nat)
{
    if (nat == NULL)
    {
        return NULL;
    }
    return nat->target;
}

cat89_status cat89_nat_component(const cat89_nat *nat, const cat89_obj *obj,
                                 cat89_mor **out_mor)
{
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (nat == NULL)
    {
        return CAT89_INVALID;
    }
    if (obj == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(cat89_functor_source(nat->source), obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (nat->ops.component == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    st = nat->ops.component(nat->ctx, obj, out_mor);
    return st;
}

/* ------------------------------------------ identity natural transformation */

static cat89_status idnat_component(void *ctx, const cat89_obj *obj,
                                    cat89_mor **out_mor)
{
    cat89_functor *functor = ctx;
    cat89_category *target;
    const cat89_obj *mapped;
    cat89_status st;

    mapped = NULL;
    st = cat89_functor_map_obj(functor, obj, &mapped);
    if (st != CAT89_OK)
    {
        return st;
    }
    target = cat89_functor_target(functor);
    st = cat89_identity(target, mapped, out_mor);
    return st;
}

static const cat89_nat_ops identity_nat_ops = {idnat_component, NULL};

cat89_status cat89_nat_identity(cat89_functor *functor,
                                const cat89_allocator *allocator,
                                cat89_nat **out_nat)
{
    cat89_status st;

    if (functor == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_nat_new(functor, functor, &identity_nat_ops, functor, allocator,
                       out_nat);
    return st;
}

/* ------------------------------------------ vertical composition */

struct vctx
{
    cat89_nat *alpha;
    cat89_nat *beta;
    cat89_category *target;
    cat89_allocator allocator;
};

static void vctx_rel(cat89_category *category, cat89_mor *mor)
{
    cat89_mor_release(category, mor);
}

static cat89_status vcomp_component(void *ctx, const cat89_obj *obj,
                                    cat89_mor **out_mor)
{
    struct vctx *vc = ctx;
    cat89_mor *aA;
    cat89_mor *bB;
    cat89_status st;

    aA = NULL;
    bB = NULL;
    st = cat89_nat_component(vc->alpha, obj, &aA);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_nat_component(vc->beta, obj, &bB);
    if (st != CAT89_OK)
    {
        vctx_rel(vc->target, aA);
        return st;
    }
    st = cat89_compose(vc->target, bB, aA, out_mor);
    vctx_rel(vc->target, bB);
    vctx_rel(vc->target, aA);
    return st;
}

static void vctx_free(void *ctx)
{
    struct vctx *vc = ctx;

    cat89_nat_release(vc->alpha);
    cat89_nat_release(vc->beta);
    cat89_free(&vc->allocator, vc);
}

static const cat89_nat_ops vertical_ops = {vcomp_component, vctx_free};

static void vctx_fail(cat89_nat *alpha, cat89_nat *beta, struct vctx *vc)
{
    cat89_nat_release(alpha);
    cat89_nat_release(beta);
    if (vc != NULL)
    {
        cat89_free(&vc->allocator, vc);
    }
}

cat89_status cat89_nat_compose_vertical(cat89_nat *beta, cat89_nat *alpha,
                                        const cat89_allocator *allocator,
                                        cat89_nat **out_nat)
{
    struct vctx *vc;
    cat89_functor *f;
    cat89_functor *h;
    cat89_functor *ta;
    cat89_functor *sb;
    cat89_category *target;
    const cat89_allocator *actual;
    cat89_status st;

    if (out_nat == NULL)
    {
        return CAT89_INVALID;
    }
    *out_nat = NULL;
    if (beta == NULL)
    {
        return CAT89_INVALID;
    }
    if (alpha == NULL)
    {
        return CAT89_INVALID;
    }
    ta = cat89_nat_target(alpha);
    sb = cat89_nat_source(beta);
    if (ta != sb)
    {
        return CAT89_INVALID;
    }

    if (allocator == NULL)
    {
        actual = cat89_allocator_default();
    }
    else
    {
        actual = allocator;
    }

    f = cat89_nat_source(alpha);
    h = cat89_nat_target(beta);
    target = cat89_functor_target(f);

    st = cat89_nat_retain(alpha);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_nat_retain(beta);
    if (st != CAT89_OK)
    {
        cat89_nat_release(alpha);
        return st;
    }

    vc = cat89_alloc(actual, sizeof(*vc));
    if (vc == NULL)
    {
        vctx_fail(alpha, beta, NULL);
        return CAT89_NOMEM;
    }
    vc->alpha = alpha;
    vc->beta = beta;
    vc->target = target;
    vc->allocator = *actual;

    st = cat89_nat_new(f, h, &vertical_ops, vc, actual, out_nat);
    if (st != CAT89_OK)
    {
        vctx_fail(alpha, beta, vc);
        return st;
    }
    return CAT89_OK;
}

/* ------------------------------------------ whiskering (left/right) */

struct wkctx
{
    cat89_functor *h;
    cat89_nat *alpha;
    cat89_category *domain;
    cat89_allocator allocator;
};

struct wrctx
{
    cat89_nat *alpha;
    cat89_functor *k;
    cat89_category *domain;
    cat89_allocator allocator;
};

static void fun_rel2(cat89_functor *a, cat89_functor *b)
{
    cat89_functor_release(a);
    cat89_functor_release(b);
}

static void fun_rel3(cat89_functor *a, cat89_functor *b, cat89_functor *c)
{
    cat89_functor_release(c);
    fun_rel2(a, b);
}

static void wk_all(cat89_functor *a, cat89_functor *b, cat89_functor *h,
                   cat89_nat *alpha)
{
    cat89_nat_release(alpha);
    fun_rel3(a, b, h);
}

static void wr_all(cat89_functor *a, cat89_functor *b, cat89_nat *alpha,
                   cat89_functor *k)
{
    cat89_nat_release(alpha);
    fun_rel3(a, b, k);
}

static void wk_rel(cat89_category *category, cat89_mor *mor)
{
    cat89_mor_release(category, mor);
}

static cat89_status wk_component(void *ctx, const cat89_obj *obj,
                                 cat89_mor **out_mor)
{
    struct wkctx *wk = ctx;
    cat89_mor *aA;
    cat89_status st;

    aA = NULL;
    st = cat89_nat_component(wk->alpha, obj, &aA);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_map_mor(wk->h, aA, out_mor);
    wk_rel(wk->domain, aA);
    return st;
}

static void wk_free(void *ctx)
{
    struct wkctx *wk = ctx;

    cat89_functor_release(wk->h);
    cat89_nat_release(wk->alpha);
    cat89_free(&wk->allocator, wk);
}

static const cat89_nat_ops whisker_left_ops = {wk_component, wk_free};

static void wk_fail(cat89_functor *h, cat89_nat *alpha, struct wkctx *wk)
{
    cat89_functor_release(h);
    cat89_nat_release(alpha);
    if (wk != NULL)
    {
        cat89_free(&wk->allocator, wk);
    }
}

cat89_status cat89_nat_whisker_left(cat89_functor *h, cat89_nat *alpha,
                                    const cat89_allocator *allocator,
                                    cat89_nat **out_nat)
{
    struct wkctx *wk;
    cat89_functor *f;
    cat89_functor *g;
    cat89_functor *src;
    cat89_functor *tgt;
    cat89_category *d;
    cat89_category *tf;
    cat89_category *sh;
    const cat89_allocator *actual;
    cat89_status st;

    if (out_nat == NULL)
    {
        return CAT89_INVALID;
    }
    *out_nat = NULL;
    if (h == NULL)
    {
        return CAT89_INVALID;
    }
    if (alpha == NULL)
    {
        return CAT89_INVALID;
    }
    f = cat89_nat_source(alpha);
    g = cat89_nat_target(alpha);
    tf = cat89_functor_target(f);
    sh = cat89_functor_source(h);
    if (tf != sh)
    {
        return CAT89_INVALID;
    }
    d = cat89_functor_target(f);

    if (allocator == NULL)
    {
        actual = cat89_allocator_default();
    }
    else
    {
        actual = allocator;
    }

    st = cat89_functor_compose(h, f, actual, &src);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_compose(h, g, actual, &tgt);
    if (st != CAT89_OK)
    {
        cat89_functor_release(src);
        return st;
    }

    st = cat89_functor_retain(h);
    if (st != CAT89_OK)
    {
        fun_rel2(src, tgt);
        return st;
    }
    st = cat89_nat_retain(alpha);
    if (st != CAT89_OK)
    {
        fun_rel3(src, tgt, h);
        return st;
    }

    wk = cat89_alloc(actual, sizeof(*wk));
    if (wk == NULL)
    {
        wk_all(src, tgt, h, alpha);
        return CAT89_NOMEM;
    }
    wk->h = h;
    wk->alpha = alpha;
    wk->domain = d;
    wk->allocator = *actual;

    st = cat89_nat_new(src, tgt, &whisker_left_ops, wk, actual, out_nat);
    cat89_functor_release(src);
    cat89_functor_release(tgt);
    if (st != CAT89_OK)
    {
        wk_fail(h, alpha, wk);
        return st;
    }
    return CAT89_OK;
}

static cat89_status wr_component(void *ctx, const cat89_obj *obj,
                                 cat89_mor **out_mor)
{
    struct wrctx *wr = ctx;
    const cat89_obj *mapped;
    cat89_status st;

    mapped = NULL;
    st = cat89_functor_map_obj(wr->k, obj, &mapped);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_nat_component(wr->alpha, mapped, out_mor);
    return st;
}

static void wr_free(void *ctx)
{
    struct wrctx *wr = ctx;

    cat89_nat_release(wr->alpha);
    cat89_functor_release(wr->k);
    cat89_free(&wr->allocator, wr);
}

static const cat89_nat_ops whisker_right_ops = {wr_component, wr_free};

static void wr_fail(cat89_nat *alpha, cat89_functor *k, struct wrctx *wr)
{
    cat89_nat_release(alpha);
    cat89_functor_release(k);
    if (wr != NULL)
    {
        cat89_free(&wr->allocator, wr);
    }
}

cat89_status cat89_nat_whisker_right(cat89_nat *alpha, cat89_functor *k,
                                     const cat89_allocator *allocator,
                                     cat89_nat **out_nat)
{
    struct wrctx *wr;
    cat89_functor *f;
    cat89_functor *g;
    cat89_functor *src;
    cat89_functor *tgt;
    cat89_category *d;
    cat89_category *tk;
    cat89_category *sf;
    const cat89_allocator *actual;
    cat89_status st;

    if (out_nat == NULL)
    {
        return CAT89_INVALID;
    }
    *out_nat = NULL;
    if (alpha == NULL)
    {
        return CAT89_INVALID;
    }
    if (k == NULL)
    {
        return CAT89_INVALID;
    }
    f = cat89_nat_source(alpha);
    g = cat89_nat_target(alpha);
    tk = cat89_functor_target(k);
    sf = cat89_functor_source(f);
    if (tk != sf)
    {
        return CAT89_INVALID;
    }
    d = cat89_functor_target(f);

    if (allocator == NULL)
    {
        actual = cat89_allocator_default();
    }
    else
    {
        actual = allocator;
    }

    st = cat89_functor_compose(f, k, actual, &src);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_compose(g, k, actual, &tgt);
    if (st != CAT89_OK)
    {
        cat89_functor_release(src);
        return st;
    }

    st = cat89_nat_retain(alpha);
    if (st != CAT89_OK)
    {
        fun_rel2(src, tgt);
        return st;
    }
    st = cat89_functor_retain(k);
    if (st != CAT89_OK)
    {
        wr_all(src, tgt, alpha, NULL);
        return st;
    }

    wr = cat89_alloc(actual, sizeof(*wr));
    if (wr == NULL)
    {
        wr_all(src, tgt, alpha, k);
        return CAT89_NOMEM;
    }
    wr->alpha = alpha;
    wr->k = k;
    wr->domain = d;
    wr->allocator = *actual;

    st = cat89_nat_new(src, tgt, &whisker_right_ops, wr, actual, out_nat);
    cat89_functor_release(src);
    cat89_functor_release(tgt);
    if (st != CAT89_OK)
    {
        wr_fail(alpha, k, wr);
        return st;
    }
    return CAT89_OK;
}
