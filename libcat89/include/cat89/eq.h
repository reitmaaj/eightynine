#ifndef CAT89_EQ_H
#define CAT89_EQ_H

#include <cat89/alloc.h>
#include <cat89/core.h>

/* cat89_eq.h - optional decidable-equality capability, bound to a category.
 *
 * Equality is orthogonal to the category core: no generic category operation
 * requires it. A checker that needs equality must be passed an explicit
 * cat89_eq. */

typedef struct cat89_eq cat89_eq;

typedef struct cat89_eq_ops
{
    cat89_status (*obj_equal)(void *ctx, const cat89_obj *a, const cat89_obj *b,
                              int *out_equal);

    cat89_status (*mor_equal)(void *ctx, const cat89_mor *f, const cat89_mor *g,
                              int *out_equal);

    void (*destroy)(void *ctx);
} cat89_eq_ops;

/* Construct an equality capability over `category`. The capability retains
 * the category. ops must be non-null; obj_equal or mor_equal may be null when
 * that direction is unsupported. allocator == NULL selects the default. */
cat89_status cat89_eq_new(cat89_category *category, const cat89_eq_ops *ops,
                          void *ctx, const cat89_allocator *allocator,
                          cat89_eq **out_eq);

/* Decide equality of objects/morphisms. out_equal receives exactly 0 or 1 and
 * is initialized to 0 before the operation. */
cat89_status cat89_obj_equal(const cat89_eq *eq, const cat89_obj *a,
                             const cat89_obj *b, int *out_equal);

cat89_status cat89_mor_equal(const cat89_eq *eq, const cat89_mor *f,
                             const cat89_mor *g, int *out_equal);

/* Borrowed category handle the capability is bound to. */
cat89_category *cat89_eq_category(const cat89_eq *eq);

/* Reference-counted lifetime. */
cat89_status cat89_eq_retain(cat89_eq *eq);

void cat89_eq_release(cat89_eq *eq);

#endif
