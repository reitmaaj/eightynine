/* cat89_eq.c - decidable-equality capability object. */

#include "cat89_internal.h"
#include <cat89/eq.h>

struct cat89_eq
{
    unsigned long refs;
    cat89_category *category;
    cat89_eq_ops ops;
    void *ctx;
    cat89_allocator allocator;
};

static void eq_init(cat89_eq *eq, cat89_category *category,
                    const cat89_eq_ops *ops, void *ctx,
                    const cat89_allocator *allocator)
{
    eq->refs = 1;
    eq->category = category;
    eq->ops = *ops;
    eq->ctx = ctx;
    eq->allocator = *allocator;
}

cat89_status cat89_eq_new(cat89_category *category, const cat89_eq_ops *ops,
                          void *ctx, const cat89_allocator *allocator,
                          cat89_eq **out_eq)
{
    cat89_eq *eq;
    cat89_status st;
    const cat89_allocator *actual;

    if (out_eq == NULL)
    {
        return CAT89_INVALID;
    }
    *out_eq = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (ops == NULL)
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

    eq = cat89_alloc(actual, sizeof(*eq));
    if (eq == NULL)
    {
        return CAT89_NOMEM;
    }

    st = cat89_category_retain(category);
    if (st != CAT89_OK)
    {
        cat89_free(actual, eq);
        return st;
    }

    eq_init(eq, category, ops, ctx, actual);
    *out_eq = eq;
    return CAT89_OK;
}

cat89_status cat89_obj_equal(const cat89_eq *eq, const cat89_obj *a,
                             const cat89_obj *b, int *out_equal)
{
    cat89_status st;

    if (out_equal == NULL)
    {
        return CAT89_INVALID;
    }
    *out_equal = 0;
    if (eq == NULL)
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
    if (cat89_owns_obj(eq->category, a) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(eq->category, b) == 0)
    {
        return CAT89_INVALID;
    }
    if (eq->ops.obj_equal == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    st = eq->ops.obj_equal(eq->ctx, a, b, out_equal);
    return st;
}

cat89_status cat89_mor_equal(const cat89_eq *eq, const cat89_mor *f,
                             const cat89_mor *g, int *out_equal)
{
    cat89_status st;

    if (out_equal == NULL)
    {
        return CAT89_INVALID;
    }
    *out_equal = 0;
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    if (g == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(eq->category, f) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(eq->category, g) == 0)
    {
        return CAT89_INVALID;
    }
    if (eq->ops.mor_equal == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    st = eq->ops.mor_equal(eq->ctx, f, g, out_equal);
    return st;
}

cat89_category *cat89_eq_category(const cat89_eq *eq)
{
    if (eq == NULL)
    {
        return NULL;
    }
    return eq->category;
}

cat89_status cat89_eq_retain(cat89_eq *eq)
{
    cat89_status st;

    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&eq->refs);
    return st;
}

static void eq_destroy(cat89_eq *eq)
{
    if (eq->ops.destroy != NULL)
    {
        eq->ops.destroy(eq->ctx);
    }
    cat89_category_release(eq->category);
    cat89_free(&eq->allocator, eq);
}

void cat89_eq_release(cat89_eq *eq)
{
    int zero;

    if (eq == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&eq->refs);
    if (zero)
    {
        eq_destroy(eq);
    }
}
