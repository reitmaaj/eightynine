/* cat89_core.c - the minimal category kernel. */

#include "cat89_internal.h"
#include <cat89/alloc.h>
#include <cat89/core.h>

struct cat89_category
{
    unsigned long refs;
    cat89_category_ops ops;
    void *ctx;
    cat89_allocator allocator;
};

static int ops_has_required(const cat89_category_ops *ops)
{
    if (ops == NULL)
    {
        return 0;
    }
    if (ops->dom == NULL)
    {
        return 0;
    }
    if (ops->cod == NULL)
    {
        return 0;
    }
    if (ops->identity == NULL)
    {
        return 0;
    }
    if (ops->compose == NULL)
    {
        return 0;
    }
    if (ops->obj_same == NULL)
    {
        return 0;
    }
    if (ops->mor_retain == NULL)
    {
        return 0;
    }
    if (ops->mor_release == NULL)
    {
        return 0;
    }
    if (ops->owns_obj == NULL)
    {
        return 0;
    }
    if (ops->owns_mor == NULL)
    {
        return 0;
    }
    return 1;
}

static void category_init(cat89_category *category,
                          const cat89_category_ops *ops, void *ctx,
                          const cat89_allocator *allocator)
{
    category->refs = 1;
    category->ops = *ops;
    category->ctx = ctx;
    category->allocator = *allocator;
}

cat89_status cat89_category_new(const cat89_category_ops *ops, void *ctx,
                                const cat89_allocator *allocator,
                                cat89_category **out_category)
{
    cat89_category *category;
    int complete;
    int use_default;
    const cat89_allocator *actual;

    if (out_category == NULL)
    {
        return CAT89_INVALID;
    }
    *out_category = NULL;

    complete = ops_has_required(ops);
    if (complete == 0)
    {
        return CAT89_INVALID;
    }

    use_default = allocator == NULL;
    if (use_default)
    {
        actual = cat89_allocator_default();
    }
    else
    {
        actual = allocator;
    }

    category = cat89_alloc(actual, sizeof(*category));
    if (category == NULL)
    {
        return CAT89_NOMEM;
    }

    category_init(category, ops, ctx, actual);
    *out_category = category;
    return CAT89_OK;
}

cat89_status cat89_category_retain(cat89_category *category)
{
    cat89_status st;

    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&category->refs);
    return st;
}

void cat89_category_release(cat89_category *category)
{
    int zero;

    if (category == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&category->refs);
    if (zero)
    {
        if (category->ops.category_destroy != NULL)
        {
            category->ops.category_destroy(category->ctx);
        }
        cat89_free(&category->allocator, category);
    }
}

cat89_status cat89_dom(const cat89_category *category, const cat89_mor *mor,
                       const cat89_obj **out_obj)
{
    cat89_status st;

    if (out_obj == NULL)
    {
        return CAT89_INVALID;
    }
    *out_obj = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (mor == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, mor) == 0)
    {
        return CAT89_INVALID;
    }
    if (category->ops.dom == NULL)
    {
        return CAT89_INVALID;
    }
    st = category->ops.dom(category->ctx, mor, out_obj);
    return st;
}

cat89_status cat89_cod(const cat89_category *category, const cat89_mor *mor,
                       const cat89_obj **out_obj)
{
    cat89_status st;

    if (out_obj == NULL)
    {
        return CAT89_INVALID;
    }
    *out_obj = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (mor == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, mor) == 0)
    {
        return CAT89_INVALID;
    }
    if (category->ops.cod == NULL)
    {
        return CAT89_INVALID;
    }
    st = category->ops.cod(category->ctx, mor, out_obj);
    return st;
}

cat89_status cat89_identity(const cat89_category *category,
                            const cat89_obj *obj, cat89_mor **out_mor)
{
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (obj == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(category, obj) == 0)
    {
        return CAT89_INVALID;
    }
    if (category->ops.identity == NULL)
    {
        return CAT89_INVALID;
    }
    st = category->ops.identity(category->ctx, obj, out_mor);
    return st;
}

cat89_status cat89_compose(const cat89_category *category, const cat89_mor *g,
                           const cat89_mor *f, cat89_mor **out_mor)
{
    cat89_status st;
    cat89_status st_cod;
    cat89_status st_dom;
    const cat89_obj *cod_f;
    const cat89_obj *dom_g;
    int same;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (g == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, g) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, f) == 0)
    {
        return CAT89_INVALID;
    }
    if (category->ops.cod == NULL)
    {
        return CAT89_INVALID;
    }
    if (category->ops.dom == NULL)
    {
        return CAT89_INVALID;
    }
    if (category->ops.obj_same == NULL)
    {
        return CAT89_INVALID;
    }
    if (category->ops.compose == NULL)
    {
        return CAT89_INVALID;
    }

    st_cod = category->ops.cod(category->ctx, f, &cod_f);
    if (st_cod != CAT89_OK)
    {
        return st_cod;
    }
    st_dom = category->ops.dom(category->ctx, g, &dom_g);
    if (st_dom != CAT89_OK)
    {
        return st_dom;
    }

    same = category->ops.obj_same(category->ctx, cod_f, dom_g);
    if (same == 0)
    {
        return CAT89_DOMAIN;
    }

    st = category->ops.compose(category->ctx, g, f, out_mor);
    return st;
}

int cat89_obj_same(const cat89_category *category, const cat89_obj *a,
                   const cat89_obj *b)
{
    int result;

    if (category == NULL)
    {
        return 0;
    }
    if (category->ops.obj_same == NULL)
    {
        return 0;
    }
    if (a == NULL)
    {
        return 0;
    }
    if (b == NULL)
    {
        return 0;
    }
    if (cat89_owns_obj(category, a) == 0)
    {
        return 0;
    }
    if (cat89_owns_obj(category, b) == 0)
    {
        return 0;
    }
    result = category->ops.obj_same(category->ctx, a, b);
    return result;
}

cat89_status cat89_mor_retain(const cat89_category *category, cat89_mor *mor)
{
    cat89_status st;

    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (mor == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, mor) == 0)
    {
        return CAT89_INVALID;
    }
    if (category->ops.mor_retain == NULL)
    {
        return CAT89_INVALID;
    }
    st = category->ops.mor_retain(category->ctx, mor);
    return st;
}

void cat89_mor_release(const cat89_category *category, cat89_mor *mor)
{
    if (category == NULL)
    {
        return;
    }
    if (mor == NULL)
    {
        return;
    }
    if (cat89_owns_mor(category, mor) == 0)
    {
        return;
    }
    if (category->ops.mor_release != NULL)
    {
        category->ops.mor_release(category->ctx, mor);
    }
}

int cat89_owns_obj(const cat89_category *category, const cat89_obj *obj)
{
    int result;

    if (category == NULL)
    {
        return 0;
    }
    if (obj == NULL)
    {
        return 0;
    }
    if (category->ops.owns_obj == NULL)
    {
        return 0;
    }
    result = category->ops.owns_obj(category->ctx, obj);
    return result != 0;
}

int cat89_owns_mor(const cat89_category *category, const cat89_mor *mor)
{
    int result;

    if (category == NULL)
    {
        return 0;
    }
    if (mor == NULL)
    {
        return 0;
    }
    if (category->ops.owns_mor == NULL)
    {
        return 0;
    }
    result = category->ops.owns_mor(category->ctx, mor);
    return result != 0;
}

void *cat89_category_ctx(const cat89_category *category)
{
    if (category == NULL)
    {
        return NULL;
    }
    return category->ctx;
}

void cat89_category_test_set_refs(cat89_category *category, unsigned long refs)
{
    if (category == NULL)
    {
        return;
    }
    category->refs = refs;
}
