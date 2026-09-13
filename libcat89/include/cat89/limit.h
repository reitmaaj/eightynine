#ifndef CAT89_LIMIT_H
#define CAT89_LIMIT_H

#include <cat89/alloc.h>
#include <cat89/cone.h>
#include <cat89/core.h>

/* cat89_limit.h - limits and colimits.
 *
 * A limit L over a diagram owns a limiting cone together with a universal
 * factor operation: for every candidate cone K over the same diagram,
 * factor(K) returns the unique mediating morphism K.apex -> L.apex through
 * which every cone leg factors. A colimit is the dual (mediating map
 * L.apex -> K.apex over cocones). Uniqueness is part of the structure's
 * contract; finite checkers verify existence and uniqueness. */

typedef struct cat89_limit cat89_limit;
typedef struct cat89_colimit cat89_colimit;

typedef struct cat89_limit_ops
{
    cat89_status (*factor)(void *ctx, const cat89_cone *candidate,
                           cat89_mor **out_mor);

    void (*destroy)(void *ctx);
} cat89_limit_ops;

/* Construct a limit whose limiting cone is `cone`. Retains the cone. */
cat89_status cat89_limit_new(cat89_cone *cone, const cat89_limit_ops *ops,
                             void *ctx, const cat89_allocator *allocator,
                             cat89_limit **out_limit);

/* For a candidate cone over the same diagram, return the unique mediating
 * morphism candidate.apex -> limit.apex (owned, in the ambient category). */
cat89_status cat89_limit_factor(const cat89_limit *limit,
                                const cat89_cone *candidate,
                                cat89_mor **out_mor);

cat89_cone *cat89_limit_cone(const cat89_limit *limit);

cat89_category *cat89_limit_category(const cat89_limit *limit);

cat89_status cat89_limit_retain(cat89_limit *limit);

void cat89_limit_release(cat89_limit *limit);

/* Colimit: owns a cocone; factor maps candidate cocones to mediating
 * morphisms colimit.apex -> candidate.apex. */
typedef struct cat89_colimit_ops
{
    cat89_status (*factor)(void *ctx, const cat89_cocone *candidate,
                           cat89_mor **out_mor);

    void (*destroy)(void *ctx);
} cat89_colimit_ops;

cat89_status cat89_colimit_new(cat89_cocone *cocone,
                               const cat89_colimit_ops *ops, void *ctx,
                               const cat89_allocator *allocator,
                               cat89_colimit **out_colimit);

cat89_status cat89_colimit_factor(const cat89_colimit *colimit,
                                  const cat89_cocone *candidate,
                                  cat89_mor **out_mor);

cat89_cocone *cat89_colimit_cocone(const cat89_colimit *colimit);

cat89_category *cat89_colimit_category(const cat89_colimit *colimit);

cat89_status cat89_colimit_retain(cat89_colimit *colimit);

void cat89_colimit_release(cat89_colimit *colimit);

#endif
