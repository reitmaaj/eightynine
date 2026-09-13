#ifndef CAT89_ARROW_H
#define CAT89_ARROW_H

#include <cat89/alloc.h>
#include <cat89/core.h>
#include <cat89/eq.h>

/* cat89_arrow.h - the arrow category C^->.
 *
 * Objects are the morphisms f : A -> B of C; a morphism from f to g is a
 * commutative square (u, v) with v o f = g o u. The arrow category is realized
 * as the comma category comma(1_C, 1_C) of the identity functor with itself, so
 * object/morphism construction inherits the comma semantics: ill-typed spans
 * are rejected (CAT89_DOMAIN) and non-commuting squares are rejected
 * (CAT89_INVALID). Object identity depends on morphism equality in C, so the
 * caller supplies an explicit cat89_eq over C. */

cat89_status cat89_arrow_category_new(cat89_category *base, cat89_eq *eq,
                                      const cat89_allocator *allocator,
                                      cat89_category **out_category);

/* Intern the arrow object f : dom -> cod of C. Borrowed result. */
cat89_status cat89_arrow_obj_new(cat89_category *arrow, const cat89_obj *dom,
                                 cat89_mor *f, const cat89_obj *cod,
                                 const cat89_obj **out_obj);

/* Build one owned arrow morphism (u, v) from dom_obj to cod_obj, verifying the
 * commuting square. Caller releases via cat89_mor_release(arrow, mor). */
cat89_status cat89_arrow_mor_new(cat89_category *arrow,
                                 const cat89_obj *dom_obj,
                                 const cat89_obj *cod_obj, cat89_mor *u,
                                 cat89_mor *v, cat89_mor **out_mor);

#endif
