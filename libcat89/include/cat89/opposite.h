#ifndef CAT89_OPPOSITE_H
#define CAT89_OPPOSITE_H

#include <cat89/alloc.h>
#include <cat89/core.h>

/* cat89_opposite.h - the opposite category C^op.
 *
 * Objects are the base objects (borrowed). A morphism is a wrapper around an
 * ordinary base morphism with domain/codomain exchanged and composition
 * reversed. Applying opposite twice yields a category canonically equivalent
 * to (not pointer-identical with) the original. */

cat89_status cat89_opposite_new(cat89_category *base,
                                const cat89_allocator *allocator,
                                cat89_category **out_category);

/* Wrap a base morphism as an opposite-category morphism (borrowed input,
 * one owned output in the opposite category). */
cat89_status cat89_opposite_mor(cat89_category *opposite, cat89_mor *base_mor,
                                cat89_mor **out_mor);

/* Borrowed underlying base morphism of an opposite morphism. NULL on NULL or
 * foreign. */
const cat89_mor *cat89_opposite_base_mor(const cat89_category *opposite,
                                         const cat89_mor *op_mor);

#endif
