#ifndef CAT89_COMMA_H
#define CAT89_COMMA_H

#include <cat89/alloc.h>
#include <cat89/core.h>
#include <cat89/eq.h>
#include <cat89/functor.h>

/* cat89_comma.h - the comma category F ↓ G for functors F : C -> E and
 * G : D -> E.
 *
 * Objects are spans (c, f, d) with c in C, d in D and f : F(c) -> G(d) in E,
 * interned by component object identity and by morphism equality in E. A
 * morphism (u, v) from (c,f,d) to (c',f',d') consists of u : c -> c' in C and
 * v : d -> d' in D satisfying the commuting square G(v) o f = f' o F(u) in E.
 *
 * Object identity and square verification depend on decidable morphism equality
 * in the target category E, which the category core must not assume (CAT-I5).
 * The caller therefore supplies an explicit cat89_eq over E, retained by the
 * comma category and used only to intern objects and to validate morphisms. */

cat89_status cat89_comma_category_new(cat89_functor *left, cat89_functor *right,
                                      cat89_eq *target_eq,
                                      const cat89_allocator *allocator,
                                      cat89_category **out_category);

/* Intern the span object (c, f, d) where f : F(c) -> G(d) in E. Returns a
 * borrowed object valid for the comma category lifetime. Rejects a span whose
 * f is not typed F(c) -> G(d) (CAT89_DOMAIN). */
cat89_status cat89_comma_obj_new(cat89_category *comma, const cat89_obj *c,
                                 cat89_mor *f, const cat89_obj *d,
                                 const cat89_obj **out_obj);

/* Build one owned comma morphism (u, v) from dom_obj to cod_obj, verifying the
 * commuting square and component typing; a non-commuting or ill-typed square
 * yields CAT89_INVALID. Caller releases via cat89_mor_release(comma, mor). */
cat89_status cat89_comma_mor_new(cat89_category *comma,
                                 const cat89_obj *dom_obj,
                                 const cat89_obj *cod_obj, cat89_mor *u,
                                 cat89_mor *v, cat89_mor **out_mor);

/* Borrowed underlying components of a comma object/morphism. NULL on NULL or
 * on a handle not owned by `comma`. */
const cat89_obj *cat89_comma_left_obj(const cat89_category *comma,
                                      const cat89_obj *obj);

const cat89_obj *cat89_comma_right_obj(const cat89_category *comma,
                                       const cat89_obj *obj);

const cat89_mor *cat89_comma_obj_mor(const cat89_category *comma,
                                     const cat89_obj *obj);

const cat89_mor *cat89_comma_left_mor(const cat89_category *comma,
                                      const cat89_mor *mor);

const cat89_mor *cat89_comma_right_mor(const cat89_category *comma,
                                       const cat89_mor *mor);

#endif
