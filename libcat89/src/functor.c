/* cat89_functor.c - functors between categories. */

#include "cat89_internal.h"
#include <cat89/functor.h>

struct cat89_functor
{
    unsigned long refs;
    cat89_category *source;
    cat89_category *target;
    cat89_functor_ops ops;
    void *ctx;
    cat89_allocator allocator;
};

static void release_cats(cat89_category *a, cat89_category *b)
{
    cat89_category_release(a);
    cat89_category_release(b);
}

static void functor_init(cat89_functor *functor, cat89_category *source,
                         cat89_category *target, const cat89_functor_ops *ops,
                         void *ctx, const cat89_allocator *allocator)
{
    functor->refs = 1;
    functor->source = source;
    functor->target = target;
    functor->ops = *ops;
    functor->ctx = ctx;
    functor->allocator = *allocator;
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

cat89_status cat89_functor_new(cat89_category *source, cat89_category *target,
                               const cat89_functor_ops *ops, void *ctx,
                               const cat89_allocator *allocator,
                               cat89_functor **out_functor)
{
    cat89_functor *functor;
    cat89_status st;
    const cat89_allocator *actual;

    if (out_functor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_functor = NULL;
    if (source == NULL)
    {
        return CAT89_INVALID;
    }
    if (target == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops == NULL)
    {
        return CAT89_INVALID;
    }

    actual = resolve_alloc(allocator);

    st = cat89_category_retain(source);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_category_retain(target);
    if (st != CAT89_OK)
    {
        cat89_category_release(source);
        return st;
    }

    functor = cat89_alloc(actual, sizeof(*functor));
    if (functor == NULL)
    {
        release_cats(source, target);
        return CAT89_NOMEM;
    }

    functor_init(functor, source, target, ops, ctx, actual);
    *out_functor = functor;
    return CAT89_OK;
}

static void functor_destroy(cat89_functor *functor)
{
    if (functor->ops.destroy != NULL)
    {
        functor->ops.destroy(functor->ctx);
    }
    cat89_category_release(functor->source);
    cat89_category_release(functor->target);
    cat89_free(&functor->allocator, functor);
}

cat89_status cat89_functor_retain(cat89_functor *functor)
{
    cat89_status st;

    if (functor == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&functor->refs);
    return st;
}

void cat89_functor_release(cat89_functor *functor)
{
    int zero;

    if (functor == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&functor->refs);
    if (zero)
    {
        functor_destroy(functor);
    }
}

cat89_category *cat89_functor_source(const cat89_functor *functor)
{
    if (functor == NULL)
    {
        return NULL;
    }
    return functor->source;
}

cat89_category *cat89_functor_target(const cat89_functor *functor)
{
    if (functor == NULL)
    {
        return NULL;
    }
    return functor->target;
}

cat89_status cat89_functor_map_obj(const cat89_functor *functor,
                                   const cat89_obj *obj,
                                   const cat89_obj **out_obj)
{
    cat89_status st;

    if (out_obj == NULL)
    {
        return CAT89_INVALID;
    }
    *out_obj = NULL;
    if (functor == NULL)
    {
        return CAT89_INVALID;
    }
    if (obj == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(functor->source, obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (functor->ops.map_obj == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    st = functor->ops.map_obj(functor->ctx, obj, out_obj);
    return st;
}

cat89_status cat89_functor_map_mor(const cat89_functor *functor, cat89_mor *mor,
                                   cat89_mor **out_mor)
{
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (functor == NULL)
    {
        return CAT89_INVALID;
    }
    if (mor == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(functor->source, mor) == 0)
    {
        return CAT89_INVALID;
    }
    if (functor->ops.map_mor == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    st = functor->ops.map_mor(functor->ctx, mor, out_mor);
    return st;
}

/* ------------------------------------------------- identity functor */

static cat89_status id_map_obj(void *ctx, const cat89_obj *obj,
                               const cat89_obj **out_obj)
{
    (void)ctx;
    *out_obj = obj;
    return CAT89_OK;
}

static cat89_status id_map_mor(void *ctx, cat89_mor *mor, cat89_mor **out_mor)
{
    cat89_category *category = ctx;
    cat89_status st;

    st = cat89_mor_retain(category, mor);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_mor = mor;
    return CAT89_OK;
}

static const cat89_functor_ops identity_ops = {id_map_obj, id_map_mor, NULL};

cat89_status cat89_functor_identity(cat89_category *category,
                                    const cat89_allocator *allocator,
                                    cat89_functor **out_functor)
{
    cat89_status st;

    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_functor_new(category, category, &identity_ops, category,
                           allocator, out_functor);
    return st;
}

/* -------------------------------------------------- composition */

struct compose_ctx
{
    cat89_functor *g;
    cat89_functor *f;
    cat89_allocator allocator;
};

static cat89_status comp_map_obj(void *ctx, const cat89_obj *obj,
                                 const cat89_obj **out_obj)
{
    struct compose_ctx *cx = ctx;
    const cat89_obj *mid;
    cat89_status st;

    mid = NULL;
    st = cat89_functor_map_obj(cx->f, obj, &mid);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_map_obj(cx->g, mid, out_obj);
    return st;
}

static cat89_status comp_map_mor(void *ctx, cat89_mor *mor, cat89_mor **out_mor)
{
    struct compose_ctx *cx = ctx;
    cat89_mor *m1;
    cat89_mor *m2;
    cat89_category *tcat;
    cat89_status st;

    m1 = NULL;
    m2 = NULL;
    st = cat89_functor_map_mor(cx->f, mor, &m1);
    if (st != CAT89_OK)
    {
        return st;
    }
    tcat = cat89_functor_target(cx->f);
    st = cat89_functor_map_mor(cx->g, m1, &m2);
    cat89_mor_release(tcat, m1);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_mor = m2;
    return CAT89_OK;
}

static void comp_destroy(void *ctx)
{
    struct compose_ctx *cx = ctx;

    cat89_functor_release(cx->g);
    cat89_functor_release(cx->f);
    cat89_free(&cx->allocator, cx);
}

static void compose_fail(cat89_functor *g, cat89_functor *f,
                         struct compose_ctx *cx, int free_ctx)
{
    cat89_functor_release(g);
    cat89_functor_release(f);
    if (free_ctx)
    {
        cat89_free(&cx->allocator, cx);
    }
}

static const cat89_functor_ops compose_ops = {comp_map_obj, comp_map_mor,
                                              comp_destroy};

cat89_status cat89_functor_compose(cat89_functor *g, cat89_functor *f,
                                   const cat89_allocator *allocator,
                                   cat89_functor **out_functor)
{
    struct compose_ctx *cx;
    cat89_category *target_f;
    cat89_category *source_g;
    cat89_category *source_f;
    const cat89_allocator *actual;
    cat89_status st;

    if (out_functor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_functor = NULL;
    if (g == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }

    target_f = cat89_functor_target(f);
    source_g = cat89_functor_source(g);
    if (target_f != source_g)
    {
        return CAT89_INVALID;
    }
    source_f = cat89_functor_source(f);

    actual = resolve_alloc(allocator);

    st = cat89_functor_retain(g);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_retain(f);
    if (st != CAT89_OK)
    {
        cat89_functor_release(g);
        return st;
    }

    cx = cat89_alloc(actual, sizeof(*cx));
    if (cx == NULL)
    {
        compose_fail(g, f, NULL, 0);
        return CAT89_NOMEM;
    }
    cx->g = g;
    cx->f = f;
    cx->allocator = *actual;

    st = cat89_functor_new(source_f, cat89_functor_target(g), &compose_ops, cx,
                           actual, out_functor);
    if (st != CAT89_OK)
    {
        compose_fail(g, f, cx, 1);
        return st;
    }
    return CAT89_OK;
}
