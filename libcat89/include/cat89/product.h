#ifndef CAT89_PRODUCT_H
#define CAT89_PRODUCT_H

#include <cat89/alloc.h>
#include <cat89/core.h>

/* cat89_product.h - the product category C x D.
 *
 * Objects are interned pairs (a, b) with a in C and b in D, owned by the
 * product category for its whole lifetime (borrowed object handles, no release
 * API). A morphism is a refcounted wrapper around an ordinary morphism of C and
 * one of D with componentwise domain, codomain, identity and composition.
 * Component equality and enumeration are NOT provided by the wrapper: equality
 * of composite objects/morphisms must be supplied externally through the
 * borrowed component accessors below (CAT-I5/I6, capability orthogonality). */

cat89_status cat89_product_category_new(cat89_category *left,
                                        cat89_category *right,
                                        const cat89_allocator *allocator,
                                        cat89_category **out_category);

/* Intern a product object (a, b). Borrowed inputs, borrowed result valid for
 * the product category lifetime. Componentwise-equal pairs reuse one token. */
cat89_status cat89_product_obj_new(cat89_category *product,
                                   const cat89_obj *left_obj,
                                   const cat89_obj *right_obj,
                                   const cat89_obj **out_obj);

/* Wrap a morphism of C and a morphism of D as one owned product morphism
 * (caller releases via cat89_mor_release(product, mor)). Inputs are borrowed;
 * the wrapper retains both. */
cat89_status cat89_product_mor_new(cat89_category *product, cat89_mor *left_mor,
                                   cat89_mor *right_mor, cat89_mor **out_mor);

/* Borrowed underlying component object/morphism of a product handle. NULL for
 * NULL input or a handle not owned by `product`. */
const cat89_obj *cat89_product_left_obj(const cat89_category *product,
                                        const cat89_obj *obj);

const cat89_obj *cat89_product_right_obj(const cat89_category *product,
                                         const cat89_obj *obj);

const cat89_mor *cat89_product_left_mor(const cat89_category *product,
                                        const cat89_mor *mor);

const cat89_mor *cat89_product_right_mor(const cat89_category *product,
                                         const cat89_mor *mor);

#endif
