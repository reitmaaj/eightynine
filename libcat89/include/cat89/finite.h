#ifndef CAT89_FINITE_H
#define CAT89_FINITE_H

#include <cat89/alloc.h>
#include <cat89/core.h>
#include <cat89/enum.h>
#include <cat89/eq.h>

/* cat89_finite.h - reference finite category backend.
 *
 * A finite category is described by a builder (objects, morphisms, identity
 * and composition assignments) and finalized into an immutable store backing
 * a built cat89_category (and, when requested, equality and enumeration
 * capabilities). Object and morphism ids are zero-based and handed out in
 * order. The caller promises the assignments describe a category; generic law
 * checking lives in check.h. */

typedef unsigned long cat89_finite_obj_id;
typedef unsigned long cat89_finite_mor_id;

typedef struct cat89_finite_builder cat89_finite_builder;

cat89_status cat89_finite_builder_new(const cat89_allocator *allocator,
                                      cat89_finite_builder **out_builder);

cat89_status cat89_finite_add_object(cat89_finite_builder *builder,
                                     cat89_finite_obj_id *out_id);

cat89_status cat89_finite_add_morphism(cat89_finite_builder *builder,
                                       cat89_finite_obj_id dom,
                                       cat89_finite_obj_id cod,
                                       cat89_finite_mor_id *out_id);

cat89_status cat89_finite_set_identity(cat89_finite_builder *builder,
                                       cat89_finite_obj_id obj,
                                       cat89_finite_mor_id mor);

cat89_status cat89_finite_set_composition(cat89_finite_builder *builder,
                                          cat89_finite_mor_id g,
                                          cat89_finite_mor_id f,
                                          cat89_finite_mor_id result);

/* Full structural validation of the finite table: every object has a typed
 * identity, every composition row references existing morphisms, is composable
 * and correctly typed, every composable pair has exactly one result, and the
 * identity and associativity laws hold. valid == 0 (not an error) when a check
 * fails; non-OK is reserved for invalid API use. */
cat89_status cat89_finite_validate(cat89_finite_builder *builder,
                                   int *out_valid);

/* Finalize into a category plus (optionally) an equality and an enumeration
 * capability over one immutable store; any out_* may be NULL to skip it. The
 * table is validated first; an invalid table yields CAT89_INVALID with all
 * outputs NULL and the builder untouched. The builder is never consumed: the
 * caller retains ownership and may release, mutate or rebuild it. */
cat89_status cat89_finite_build(cat89_finite_builder *builder,
                                cat89_category **out_category,
                                cat89_eq **out_eq, cat89_enum **out_enum);

void cat89_finite_builder_release(cat89_finite_builder *builder);

#endif
