#ifndef CAT89_ISO_H
#define CAT89_ISO_H

#include <cat89/alloc.h>
#include <cat89/core.h>

/* cat89_iso.h - isomorphisms as packaged ordinary morphisms (CAT-I3).
 *
 * An isomorphism A -> B is a forward morphism f : A -> B together with an
 * inverse morphism g : B -> A promised to satisfy g o f = 1_A and f o g = 1_B.
 * The two morphisms are ordinary cat89_mor values; the iso adds no subtype or
 * flag. No inverse-law verification occurs automatically; check.h supplies
 * cat89_check_iso. */

typedef struct cat89_iso cat89_iso;

/* Construct an iso from a forward and an inverse morphism in `category`.
 * Retains the category and both morphisms (caller keeps its own references).
 * op == NULL selects the default allocator. */
cat89_status cat89_iso_new(cat89_category *category, cat89_mor *forward,
                           cat89_mor *inverse, const cat89_allocator *allocator,
                           cat89_iso **out_iso);

/* Borrowed (non-const) accessors; the iso retains ownership of the morphisms
 * and the caller must not release them. */
cat89_category *cat89_iso_category(const cat89_iso *iso);

cat89_mor *cat89_iso_forward(const cat89_iso *iso);

cat89_mor *cat89_iso_inverse(const cat89_iso *iso);

/* Identity isomorphism on an object: (1_A, 1_A). */
cat89_status cat89_iso_identity(cat89_category *category, const cat89_obj *obj,
                                const cat89_allocator *allocator,
                                cat89_iso **out_iso);

/* Invert: swap forward and inverse. */
cat89_status cat89_iso_invert(const cat89_iso *iso,
                              const cat89_allocator *allocator,
                              cat89_iso **out_iso);

/* Compose: out = g o f (category instances must match). */
cat89_status cat89_iso_compose(const cat89_iso *g, const cat89_iso *f,
                               const cat89_allocator *allocator,
                               cat89_iso **out_iso);

cat89_status cat89_iso_retain(cat89_iso *iso);

void cat89_iso_release(cat89_iso *iso);

#endif
