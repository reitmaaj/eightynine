/* cat89_cone.c - cones and cocones over a diagram. */

#include "cat89_internal.h"
#include <cat89/cone.h>

struct cat89_cone
{
    unsigned long refs;
    cat89_diagram *diagram;
    cat89_category *category;
    const cat89_obj *apex;
    cat89_cone_ops ops;
    void *ctx;
    cat89_allocator allocator;
};

struct cat89_cocone
{
    unsigned long refs;
    cat89_diagram *diagram;
    cat89_category *category;
    const cat89_obj *apex;
    cat89_cone_ops ops;
    void *ctx;
    cat89_allocator allocator;
};

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

/* cone ------------------------------------------------------------ */

cat89_status cat89_cone_new(cat89_diagram *diagram, const cat89_obj *apex,
                            const cat89_cone_ops *ops, void *ctx,
                            const cat89_allocator *allocator,
                            cat89_cone **out_cone)
{
    cat89_cone *cone;
    cat89_category *category;
    const cat89_allocator *actual;
    cat89_status st;

    if (out_cone == NULL)
    {
        return CAT89_INVALID;
    }
    *out_cone = NULL;
    if (diagram == NULL)
    {
        return CAT89_INVALID;
    }
    if (apex == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops->leg == NULL)
    {
        return CAT89_INVALID;
    }
    category = cat89_functor_target(diagram);
    if (cat89_owns_obj(category, apex) == 0)
    {
        return CAT89_INVALID;
    }
    actual = resolve_alloc(allocator);

    st = cat89_functor_retain(diagram);
    if (st != CAT89_OK)
    {
        return st;
    }

    cone = cat89_alloc(actual, sizeof(*cone));
    if (cone == NULL)
    {
        cat89_functor_release(diagram);
        return CAT89_NOMEM;
    }
    cone->refs = 1;
    cone->diagram = diagram;
    cone->category = category;
    cone->apex = apex;
    cone->ops = *ops;
    cone->ctx = ctx;
    cone->allocator = *actual;
    *out_cone = cone;
    return CAT89_OK;
}

static void cone_destroy(cat89_cone *cone)
{
    if (cone->ops.destroy != NULL)
    {
        cone->ops.destroy(cone->ctx);
    }
    cat89_functor_release(cone->diagram);
    cat89_free(&cone->allocator, cone);
}

cat89_status cat89_cone_retain(cat89_cone *cone)
{
    cat89_status st;

    if (cone == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&cone->refs);
    return st;
}

void cat89_cone_release(cat89_cone *cone)
{
    int zero;

    if (cone == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&cone->refs);
    if (zero)
    {
        cone_destroy(cone);
    }
}

cat89_status cat89_cone_leg(const cat89_cone *cone, const cat89_obj *shape_obj,
                            cat89_mor **out_mor)
{
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (cone == NULL)
    {
        return CAT89_INVALID;
    }
    if (shape_obj == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(cat89_functor_source(cone->diagram), shape_obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (cone->ops.leg == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    st = cone->ops.leg(cone->ctx, shape_obj, out_mor);
    return st;
}

cat89_diagram *cat89_cone_diagram(const cat89_cone *cone)
{
    if (cone == NULL)
    {
        return NULL;
    }
    return cone->diagram;
}

const cat89_obj *cat89_cone_apex(const cat89_cone *cone)
{
    if (cone == NULL)
    {
        return NULL;
    }
    return cone->apex;
}

cat89_category *cat89_cone_category(const cat89_cone *cone)
{
    if (cone == NULL)
    {
        return NULL;
    }
    return cone->category;
}

/* cocone ---------------------------------------------------------- */

cat89_status cat89_cocone_new(cat89_diagram *diagram, const cat89_obj *apex,
                              const cat89_cone_ops *ops, void *ctx,
                              const cat89_allocator *allocator,
                              cat89_cocone **out_cocone)
{
    cat89_cocone *cocone;
    cat89_category *category;
    const cat89_allocator *actual;
    cat89_status st;

    if (out_cocone == NULL)
    {
        return CAT89_INVALID;
    }
    *out_cocone = NULL;
    if (diagram == NULL)
    {
        return CAT89_INVALID;
    }
    if (apex == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops->leg == NULL)
    {
        return CAT89_INVALID;
    }
    category = cat89_functor_target(diagram);
    if (cat89_owns_obj(category, apex) == 0)
    {
        return CAT89_INVALID;
    }
    actual = resolve_alloc(allocator);

    st = cat89_functor_retain(diagram);
    if (st != CAT89_OK)
    {
        return st;
    }

    cocone = cat89_alloc(actual, sizeof(*cocone));
    if (cocone == NULL)
    {
        cat89_functor_release(diagram);
        return CAT89_NOMEM;
    }
    cocone->refs = 1;
    cocone->diagram = diagram;
    cocone->category = category;
    cocone->apex = apex;
    cocone->ops = *ops;
    cocone->ctx = ctx;
    cocone->allocator = *actual;
    *out_cocone = cocone;
    return CAT89_OK;
}

static void cocone_destroy(cat89_cocone *cocone)
{
    if (cocone->ops.destroy != NULL)
    {
        cocone->ops.destroy(cocone->ctx);
    }
    cat89_functor_release(cocone->diagram);
    cat89_free(&cocone->allocator, cocone);
}

cat89_status cat89_cocone_retain(cat89_cocone *cocone)
{
    cat89_status st;

    if (cocone == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&cocone->refs);
    return st;
}

void cat89_cocone_release(cat89_cocone *cocone)
{
    int zero;

    if (cocone == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&cocone->refs);
    if (zero)
    {
        cocone_destroy(cocone);
    }
}

cat89_status cat89_cocone_leg(const cat89_cocone *cocone,
                              const cat89_obj *shape_obj, cat89_mor **out_mor)
{
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (cocone == NULL)
    {
        return CAT89_INVALID;
    }
    if (shape_obj == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(cat89_functor_source(cocone->diagram), shape_obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (cocone->ops.leg == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    st = cocone->ops.leg(cocone->ctx, shape_obj, out_mor);
    return st;
}

cat89_diagram *cat89_cocone_diagram(const cat89_cocone *cocone)
{
    if (cocone == NULL)
    {
        return NULL;
    }
    return cocone->diagram;
}

const cat89_obj *cat89_cocone_apex(const cat89_cocone *cocone)
{
    if (cocone == NULL)
    {
        return NULL;
    }
    return cocone->apex;
}

cat89_category *cat89_cocone_category(const cat89_cocone *cocone)
{
    if (cocone == NULL)
    {
        return NULL;
    }
    return cocone->category;
}
