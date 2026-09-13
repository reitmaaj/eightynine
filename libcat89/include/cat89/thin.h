#ifndef CAT89_THIN_H
#define CAT89_THIN_H

#include <cat89/alloc.h>
#include <cat89/core.h>

/* cat89_thin.h - thin category derived from a preorder.
 *
 * A thin category has at most one morphism a -> b for any pair of objects;
 * such a morphism exists exactly when a <= b under the supplied preorder. The
 * preorder provider (not libcat89) defines the relation. Object handles are
 * per-value tokens owned by the category and valid for its lifetime. */

typedef struct cat89_preorder_ops
{
    int (*same)(void *ctx, const void *a, const void *b);

    cat89_status (*leq)(void *ctx, const void *a, const void *b, int *out_leq);
} cat89_preorder_ops;

/* Build a thin category from a preorder over opaque values. same and leq are
 * required. ctx is passed through to both callbacks. */
cat89_status cat89_thin_category_new(const cat89_preorder_ops *ops, void *ctx,
                                     const cat89_allocator *allocator,
                                     cat89_category **out_category);

/* Return a borrowed object handle for `value` (owned by the category). */
cat89_status cat89_thin_obj(cat89_category *category, const void *value,
                            const cat89_obj **out_obj);

/* If a <= b, return one owned morphism a -> b; otherwise CAT89_NOT_FOUND
 * (with *out == NULL). */
cat89_status cat89_thin_mor(cat89_category *category, const cat89_obj *a,
                            const cat89_obj *b, cat89_mor **out_mor);

#endif
