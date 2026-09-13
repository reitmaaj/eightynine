#ifndef CAT89_FUNCTOR_H
#define CAT89_FUNCTOR_H

#include <cat89/alloc.h>
#include <cat89/core.h>

/* cat89_functor.h - functors between categories.
 *
 * A functor F : C -> D retains both categories and supplies object and
 * morphism mappings. map_obj returns a borrowed target object; map_mor returns
 * one owned target morphism the caller releases through the target category.
 * The mapped morphism argument is non-const so a functor may retain/copy it.
 * Functor laws (identity/composition preservation) are promises of the
 * provider; check.h supplies explicit checkers. */

typedef struct cat89_functor cat89_functor;

typedef struct cat89_functor_ops
{
    cat89_status (*map_obj)(void *ctx, const cat89_obj *obj,
                            const cat89_obj **out_obj);

    cat89_status (*map_mor)(void *ctx, cat89_mor *mor, cat89_mor **out_mor);

    void (*destroy)(void *ctx);
} cat89_functor_ops;

/* Construct a functor source -> target. Retains both categories. ops must be
 * non-null; map_obj/map_mor may individually be null (-> CAT89_NOT_SUPPORTED
 * when invoked). */
cat89_status cat89_functor_new(cat89_category *source, cat89_category *target,
                               const cat89_functor_ops *ops, void *ctx,
                               const cat89_allocator *allocator,
                               cat89_functor **out_functor);

/* Borrowed source/target category handles (retained by the functor). */
cat89_category *cat89_functor_source(const cat89_functor *functor);

cat89_category *cat89_functor_target(const cat89_functor *functor);

/* Object mapping: borrowed result in the target category. */
cat89_status cat89_functor_map_obj(const cat89_functor *functor,
                                   const cat89_obj *obj,
                                   const cat89_obj **out_obj);

/* Morphism mapping: one owned result in the target category. */
cat89_status cat89_functor_map_mor(const cat89_functor *functor, cat89_mor *mor,
                                   cat89_mor **out_mor);

/* Identity functor 1_C : C -> C (object handle preserved; morphism mapping
 * returns a retained reference). */
cat89_status cat89_functor_identity(cat89_category *category,
                                    const cat89_allocator *allocator,
                                    cat89_functor **out_functor);

/* Functor composition: out = g o f : source(f) -> target(g). Requires
 * target(f) and source(g) to be the same category instance; otherwise
 * CAT89_INVALID. */
cat89_status cat89_functor_compose(cat89_functor *g, cat89_functor *f,
                                   const cat89_allocator *allocator,
                                   cat89_functor **out_functor);

cat89_status cat89_functor_retain(cat89_functor *functor);

void cat89_functor_release(cat89_functor *functor);

#endif
