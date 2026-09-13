#ifndef CAT89_COSLICE_H
#define CAT89_COSLICE_H

#include <cat89/alloc.h>
#include <cat89/core.h>
#include <cat89/eq.h>

/* cat89_coslice.h - the coslice category A/C over an object A of C.
 *
 * Objects are morphisms f : A -> X of C with domain A. A morphism from
 * f : A -> X to g : A -> Y is k : X -> Y making the triangle k o f = g commute.
 * Object identity is morphism equality in C, so the caller supplies an explicit
 * cat89_eq over C (CAT-I5). Objects are interned by that equality and borrowed
 * for the coslice-category lifetime; a morphism is a refcounted wrapper. */

cat89_status cat89_coslice_category_new(cat89_category *category, cat89_eq *eq,
                                        const cat89_obj *source,
                                        const cat89_allocator *allocator,
                                        cat89_category **out_category);

/* Intern the coslice object f (must have domain `source`; else CAT89_DOMAIN).
 */
cat89_status cat89_coslice_obj_new(cat89_category *coslice, cat89_mor *f,
                                   const cat89_obj **out_obj);

/* Build one owned coslice morphism k from dom_obj (f) to cod_obj (g) verifying
 * k o f = g and the typing. Caller releases via cat89_mor_release(coslice,
 * mor). */
cat89_status cat89_coslice_mor_new(cat89_category *coslice,
                                   const cat89_obj *dom_obj,
                                   const cat89_obj *cod_obj, cat89_mor *k,
                                   cat89_mor **out_mor);

/* Borrowed underlying morphism of a coslice object/morphism (NULL on NULL or
 * foreign). */
const cat89_mor *cat89_coslice_obj_mor(const cat89_category *coslice,
                                       const cat89_obj *obj);

const cat89_mor *cat89_coslice_mor_mor(const cat89_category *coslice,
                                       const cat89_mor *mor);

#endif
