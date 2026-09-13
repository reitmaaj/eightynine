/* cat89_enum.c - enumeration capability object. */

#include "cat89_internal.h"
#include <cat89/enum.h>

struct cat89_enum
{
    unsigned long refs;
    cat89_category *category;
    cat89_enum_ops ops;
    void *ctx;
    cat89_allocator allocator;
};

static void enum_init(cat89_enum *enumeration, cat89_category *category,
                      const cat89_enum_ops *ops, void *ctx,
                      const cat89_allocator *allocator)
{
    enumeration->refs = 1;
    enumeration->category = category;
    enumeration->ops = *ops;
    enumeration->ctx = ctx;
    enumeration->allocator = *allocator;
}

cat89_status cat89_enum_new(cat89_category *category, const cat89_enum_ops *ops,
                            void *ctx, const cat89_allocator *allocator,
                            cat89_enum **out_enum)
{
    cat89_enum *enumeration;
    cat89_status st;
    const cat89_allocator *actual;

    if (out_enum == NULL)
    {
        return CAT89_INVALID;
    }
    *out_enum = NULL;
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

    enumeration = cat89_alloc(actual, sizeof(*enumeration));
    if (enumeration == NULL)
    {
        return CAT89_NOMEM;
    }

    st = cat89_category_retain(category);
    if (st != CAT89_OK)
    {
        cat89_free(actual, enumeration);
        return st;
    }

    enum_init(enumeration, category, ops, ctx, actual);
    *out_enum = enumeration;
    return CAT89_OK;
}

cat89_status cat89_obj_iter_open(const cat89_enum *enumeration,
                                 cat89_obj_iter **out_iter)
{
    cat89_status st;

    if (out_iter == NULL)
    {
        return CAT89_INVALID;
    }
    *out_iter = NULL;
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration->ops.obj_iter_open == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    st = enumeration->ops.obj_iter_open(enumeration->ctx, out_iter);
    return st;
}

cat89_status cat89_obj_iter_next(const cat89_enum *enumeration,
                                 cat89_obj_iter *iter,
                                 const cat89_obj **out_obj, int *out_done)
{
    cat89_status st;

    if (out_done == NULL)
    {
        return CAT89_INVALID;
    }
    *out_done = 0;
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration->ops.obj_iter_next == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (iter == NULL)
    {
        return CAT89_INVALID;
    }
    st = enumeration->ops.obj_iter_next(enumeration->ctx, iter, out_obj,
                                        out_done);
    return st;
}

void cat89_obj_iter_close(const cat89_enum *enumeration, cat89_obj_iter *iter)
{
    if (enumeration == NULL)
    {
        return;
    }
    if (enumeration->ops.obj_iter_close == NULL)
    {
        return;
    }
    enumeration->ops.obj_iter_close(enumeration->ctx, iter);
}

cat89_status cat89_mor_iter_open(const cat89_enum *enumeration,
                                 cat89_mor_iter **out_iter)
{
    cat89_status st;

    if (out_iter == NULL)
    {
        return CAT89_INVALID;
    }
    *out_iter = NULL;
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration->ops.mor_iter_open == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    st = enumeration->ops.mor_iter_open(enumeration->ctx, out_iter);
    return st;
}

cat89_status cat89_mor_iter_next(const cat89_enum *enumeration,
                                 cat89_mor_iter *iter, cat89_mor **out_mor,
                                 int *out_done)
{
    cat89_status st;

    if (out_done == NULL)
    {
        return CAT89_INVALID;
    }
    *out_done = 0;
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration->ops.mor_iter_next == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (iter == NULL)
    {
        return CAT89_INVALID;
    }
    st = enumeration->ops.mor_iter_next(enumeration->ctx, iter, out_mor,
                                        out_done);
    return st;
}

void cat89_mor_iter_close(const cat89_enum *enumeration, cat89_mor_iter *iter)
{
    if (enumeration == NULL)
    {
        return;
    }
    if (enumeration->ops.mor_iter_close == NULL)
    {
        return;
    }
    enumeration->ops.mor_iter_close(enumeration->ctx, iter);
}

cat89_status cat89_hom_iter_open(const cat89_enum *enumeration,
                                 const cat89_obj *dom, const cat89_obj *cod,
                                 cat89_mor_iter **out_iter)
{
    cat89_status st;

    if (out_iter == NULL)
    {
        return CAT89_INVALID;
    }
    *out_iter = NULL;
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (dom == NULL)
    {
        return CAT89_INVALID;
    }
    if (cod == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(enumeration->category, dom) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(enumeration->category, cod) == 0)
    {
        return CAT89_INVALID;
    }
    if (enumeration->ops.hom_iter_open == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    st = enumeration->ops.hom_iter_open(enumeration->ctx, dom, cod, out_iter);
    return st;
}

cat89_status cat89_hom_iter_next(const cat89_enum *enumeration,
                                 cat89_mor_iter *iter, cat89_mor **out_mor,
                                 int *out_done)
{
    cat89_status st;

    if (out_done == NULL)
    {
        return CAT89_INVALID;
    }
    *out_done = 0;
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration->ops.mor_iter_next == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (iter == NULL)
    {
        return CAT89_INVALID;
    }
    st = enumeration->ops.mor_iter_next(enumeration->ctx, iter, out_mor,
                                        out_done);
    return st;
}

void cat89_hom_iter_close(const cat89_enum *enumeration, cat89_mor_iter *iter)
{
    if (enumeration == NULL)
    {
        return;
    }
    if (enumeration->ops.mor_iter_close == NULL)
    {
        return;
    }
    enumeration->ops.mor_iter_close(enumeration->ctx, iter);
}

cat89_category *cat89_enum_category(const cat89_enum *enumeration)
{
    if (enumeration == NULL)
    {
        return NULL;
    }
    return enumeration->category;
}

cat89_status cat89_enum_retain(cat89_enum *enumeration)
{
    cat89_status st;

    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&enumeration->refs);
    return st;
}

static void enum_destroy(cat89_enum *enumeration)
{
    if (enumeration->ops.destroy != NULL)
    {
        enumeration->ops.destroy(enumeration->ctx);
    }
    cat89_category_release(enumeration->category);
    cat89_free(&enumeration->allocator, enumeration);
}

void cat89_enum_release(cat89_enum *enumeration)
{
    int zero;

    if (enumeration == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&enumeration->refs);
    if (zero)
    {
        enum_destroy(enumeration);
    }
}
