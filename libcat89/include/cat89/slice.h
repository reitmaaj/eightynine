#ifndef CAT89_SLICE_H
#define CAT89_SLICE_H

#include <cat89/alloc.h>
#include <cat89/core.h>
#include <cat89/eq.h>

/* cat89_slice.h - the slice category C/A over an object A of C.
 *
 * Objects are morphisms f : X -> A of C with codomain A. A morphism from
 * f : X -> A to g : Y -> A is h : X -> Y making the triangle g o h = f commute.
 * Object identity is morphism equality in C, so the caller supplies an explicit
 * cat89_eq over C (CAT-I5). Objects are interned by that equality and borrowed
 * for the slice-category lifetime; a morphism is a refcounted wrapper. */

cat89_status cat89_slice_category_new(cat89_category *category, cat89_eq *eq,
                                      const cat89_obj *apex,
                                      const cat89_allocator *allocator,
                                      cat89_category **out_category);

/* Intern the slice object f (must have codomain A; else CAT89_DOMAIN). */
cat89_status cat89_slice_obj_new(cat89_category *slice, cat89_mor *f,
                                 const cat89_obj **out_obj);

/* Build one owned slice morphism h from dom_obj (f) to cod_obj (g) verifying
 * g o h = f and the typing. Caller releases via cat89_mor_release(slice, mor).
 */
cat89_status cat89_slice_mor_new(cat89_category *slice,
                                 const cat89_obj *dom_obj,
                                 const cat89_obj *cod_obj, cat89_mor *h,
                                 cat89_mor **out_mor);

/* Borrowed underlying morphism of a slice object (NULL on NULL or foreign). */
const cat89_mor *cat89_slice_obj_mor(const cat89_category *slice,
                                     const cat89_obj *obj);

/* Borrowed underlying morphism of a slice morphism (NULL on NULL or foreign).
 */
const cat89_mor *cat89_slice_mor_mor(const cat89_category *slice,
                                     const cat89_mor *mor);

#endif
