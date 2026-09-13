#ifndef CAT89_NAT_H
#define CAT89_NAT_H

#include <cat89/alloc.h>
#include <cat89/core.h>
#include <cat89/functor.h>

/* cat89_nat.h - natural transformations between functors.
 *
 * A natural transformation eta : F => G with F, G : C -> D assigns each object
 * A of C a morphism eta_A : F(A) -> G(A) in D, promised to be natural (the
 * provider contract). component returns one owned morphism in the target
 * category of the two functors. Generic composition (vertical/whiskering/
 * horizontal) and the naturality checker are layered on top of this core. */

typedef struct cat89_nat cat89_nat;

typedef struct cat89_nat_ops
{
    cat89_status (*component)(void *ctx, const cat89_obj *obj,
                              cat89_mor **out_mor);

    void (*destroy)(void *ctx);
} cat89_nat_ops;

/* Construct eta : source_functor => target_functor. Requires the two functors
 * to share source and target categories (instance identity). Retains both
 * functors. ops must be non-null with a component callback. */
cat89_status cat89_nat_new(cat89_functor *source_functor,
                           cat89_functor *target_functor,
                           const cat89_nat_ops *ops, void *ctx,
                           const cat89_allocator *allocator,
                           cat89_nat **out_nat);

/* Borrowed source/target functors. */
cat89_functor *cat89_nat_source(const cat89_nat *nat);

cat89_functor *cat89_nat_target(const cat89_nat *nat);

/* Component at object A of the shared source category: one owned morphism
 * F(A) -> G(A) in the target category. */
cat89_status cat89_nat_component(const cat89_nat *nat, const cat89_obj *obj,
                                 cat89_mor **out_mor);

/* Identity natural transformation 1_F : F => F: component_A = 1_{F(A)}. */
cat89_status cat89_nat_identity(cat89_functor *functor,
                                const cat89_allocator *allocator,
                                cat89_nat **out_nat);

cat89_status cat89_nat_retain(cat89_nat *nat);

void cat89_nat_release(cat89_nat *nat);

/* Vertical composition: out = beta o alpha : source(alpha) => target(beta).
 * Requires target(alpha) == source(beta) (functor instance). Component A is
 * beta_A o alpha_A in the shared target category. */
cat89_status cat89_nat_compose_vertical(cat89_nat *beta, cat89_nat *alpha,
                                        const cat89_allocator *allocator,
                                        cat89_nat **out_nat);

/* Left whiskering by functor H (D -> E) of alpha : F => G (C -> D):
 * component_A = H(alpha_A); result H o F => H o G over C -> E. */
cat89_status cat89_nat_whisker_left(cat89_functor *h, cat89_nat *alpha,
                                    const cat89_allocator *allocator,
                                    cat89_nat **out_nat);

/* Right whiskering of alpha : F => G (C -> D) by functor K (C' -> C):
 * component_X = alpha_{K(X)}; result F o K => G o K over C' -> D. */
cat89_status cat89_nat_whisker_right(cat89_nat *alpha, cat89_functor *k,
                                     const cat89_allocator *allocator,
                                     cat89_nat **out_nat);

#endif
