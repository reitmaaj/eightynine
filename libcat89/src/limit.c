/* cat89_limit.c - limits and colimits. */

#include "cat89_internal.h"
#include <cat89/limit.h>

struct cat89_limit
{
    unsigned long refs;
    cat89_cone *cone;
    cat89_category *category;
    cat89_limit_ops ops;
    void *ctx;
    cat89_allocator allocator;
};

struct cat89_colimit
{
    unsigned long refs;
    cat89_cocone *cocone;
    cat89_category *category;
    cat89_colimit_ops ops;
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

/* limit ------------------------------------------------------------- */

cat89_status cat89_limit_new(cat89_cone *cone, const cat89_limit_ops *ops,
                             void *ctx, const cat89_allocator *allocator,
                             cat89_limit **out_limit)
{
    cat89_limit *limit;
    const cat89_allocator *actual;
    cat89_status st;

    if (out_limit == NULL)
    {
        return CAT89_INVALID;
    }
    *out_limit = NULL;
    if (cone == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops->factor == NULL)
    {
        return CAT89_INVALID;
    }
    actual = resolve_alloc(allocator);

    st = cat89_cone_retain(cone);
    if (st != CAT89_OK)
    {
        return st;
    }

    limit = cat89_alloc(actual, sizeof(*limit));
    if (limit == NULL)
    {
        cat89_cone_release(cone);
        return CAT89_NOMEM;
    }
    limit->refs = 1;
    limit->cone = cone;
    limit->category = cat89_cone_category(cone);
    limit->ops = *ops;
    limit->ctx = ctx;
    limit->allocator = *actual;
    *out_limit = limit;
    return CAT89_OK;
}

static void limit_destroy(cat89_limit *limit)
{
    if (limit->ops.destroy != NULL)
    {
        limit->ops.destroy(limit->ctx);
    }
    cat89_cone_release(limit->cone);
    cat89_free(&limit->allocator, limit);
}

cat89_status cat89_limit_retain(cat89_limit *limit)
{
    cat89_status st;

    if (limit == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&limit->refs);
    return st;
}

void cat89_limit_release(cat89_limit *limit)
{
    int zero;

    if (limit == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&limit->refs);
    if (zero)
    {
        limit_destroy(limit);
    }
}

cat89_status cat89_limit_factor(const cat89_limit *limit,
                                const cat89_cone *candidate,
                                cat89_mor **out_mor)
{
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (limit == NULL)
    {
        return CAT89_INVALID;
    }
    if (candidate == NULL)
    {
        return CAT89_INVALID;
    }
    if (limit->ops.factor == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (cat89_cone_diagram(candidate) !=
        cat89_cone_diagram(cat89_limit_cone(limit)))
    {
        return CAT89_INVALID;
    }
    st = limit->ops.factor(limit->ctx, candidate, out_mor);
    return st;
}

cat89_cone *cat89_limit_cone(const cat89_limit *limit)
{
    if (limit == NULL)
    {
        return NULL;
    }
    return limit->cone;
}

cat89_category *cat89_limit_category(const cat89_limit *limit)
{
    if (limit == NULL)
    {
        return NULL;
    }
    return limit->category;
}

/* colimit ----------------------------------------------------------- */

cat89_status cat89_colimit_new(cat89_cocone *cocone,
                               const cat89_colimit_ops *ops, void *ctx,
                               const cat89_allocator *allocator,
                               cat89_colimit **out_colimit)
{
    cat89_colimit *colimit;
    const cat89_allocator *actual;
    cat89_status st;

    if (out_colimit == NULL)
    {
        return CAT89_INVALID;
    }
    *out_colimit = NULL;
    if (cocone == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops->factor == NULL)
    {
        return CAT89_INVALID;
    }
    actual = resolve_alloc(allocator);

    st = cat89_cocone_retain(cocone);
    if (st != CAT89_OK)
    {
        return st;
    }

    colimit = cat89_alloc(actual, sizeof(*colimit));
    if (colimit == NULL)
    {
        cat89_cocone_release(cocone);
        return CAT89_NOMEM;
    }
    colimit->refs = 1;
    colimit->cocone = cocone;
    colimit->category = cat89_cocone_category(cocone);
    colimit->ops = *ops;
    colimit->ctx = ctx;
    colimit->allocator = *actual;
    *out_colimit = colimit;
    return CAT89_OK;
}

static void colimit_destroy(cat89_colimit *colimit)
{
    if (colimit->ops.destroy != NULL)
    {
        colimit->ops.destroy(colimit->ctx);
    }
    cat89_cocone_release(colimit->cocone);
    cat89_free(&colimit->allocator, colimit);
}

cat89_status cat89_colimit_retain(cat89_colimit *colimit)
{
    cat89_status st;

    if (colimit == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&colimit->refs);
    return st;
}

void cat89_colimit_release(cat89_colimit *colimit)
{
    int zero;

    if (colimit == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&colimit->refs);
    if (zero)
    {
        colimit_destroy(colimit);
    }
}

cat89_status cat89_colimit_factor(const cat89_colimit *colimit,
                                  const cat89_cocone *candidate,
                                  cat89_mor **out_mor)
{
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (colimit == NULL)
    {
        return CAT89_INVALID;
    }
    if (candidate == NULL)
    {
        return CAT89_INVALID;
    }
    if (colimit->ops.factor == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (cat89_cocone_diagram(candidate) !=
        cat89_cocone_diagram(cat89_colimit_cocone(colimit)))
    {
        return CAT89_INVALID;
    }
    st = colimit->ops.factor(colimit->ctx, candidate, out_mor);
    return st;
}

cat89_cocone *cat89_colimit_cocone(const cat89_colimit *colimit)
{
    if (colimit == NULL)
    {
        return NULL;
    }
    return colimit->cocone;
}

cat89_category *cat89_colimit_category(const cat89_colimit *colimit)
{
    if (colimit == NULL)
    {
        return NULL;
    }
    return colimit->category;
}
