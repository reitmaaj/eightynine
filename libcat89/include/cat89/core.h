#ifndef CAT89_CORE_H
#define CAT89_CORE_H

#include <cat89/alloc.h>
#include <cat89/status.h>

/* cat89_core.h - the minimal category kernel.
 *
 * A category is represented by opaque handles (cat89_category, cat89_obj,
 * cat89_mor). A backend supplies the category_ops below and promises the
 * categorical laws (identity, associativity); the core enforces only the
 * mechanical typing captured by dom/cod/compose.
 *
 * Core.h must never include a higher-level header.
 */

typedef struct cat89_category cat89_category;
typedef struct cat89_obj cat89_obj;
typedef struct cat89_mor cat89_mor;

/* Category backend callbacks.
 *
 * dom/cod return a borrowed object handle.
 * identity returns one owned morphism (caller must release).
 * compose returns one owned morphism for g o f.
 * obj_same is structural object identity for typing; it must be an
 *   equivalence relation, must not allocate, and must not fail. It is NOT
 *   general categorical equality.
 * mor_retain/mor_release provide reference-style morphism lifetime.
 * owns_obj/owns_mor report whether a live handle presently belongs to this
 *   category (provenance, not semantic equality and not exclusive ownership).
 *   Both must be safe on any live foreign handle: they must not dereference
 *   or inspect the candidate, retain/release it, or call another
 *   representation-specific operation on it. Comparing the candidate pointer
 *   against pointers already known to the backend is permitted. NULL yields 0.
 * category_destroy is optional; all others are required.
 */
typedef struct cat89_category_ops
{
    cat89_status (*dom)(void *ctx, const cat89_mor *mor,
                        const cat89_obj **out_obj);

    cat89_status (*cod)(void *ctx, const cat89_mor *mor,
                        const cat89_obj **out_obj);

    cat89_status (*identity)(void *ctx, const cat89_obj *obj,
                             cat89_mor **out_mor);

    cat89_status (*compose)(void *ctx, const cat89_mor *g, const cat89_mor *f,
                            cat89_mor **out_mor);

    int (*obj_same)(void *ctx, const cat89_obj *a, const cat89_obj *b);

    cat89_status (*mor_retain)(void *ctx, cat89_mor *mor);

    void (*mor_release)(void *ctx, cat89_mor *mor);

    int (*owns_obj)(void *ctx, const cat89_obj *obj);

    int (*owns_mor)(void *ctx, const cat89_mor *mor);

    void (*category_destroy)(void *ctx);
} cat89_category_ops;

/* Construct a category. ops and its required callbacks must be non-null;
 * allocator == NULL selects the default allocator. On success *out != NULL.
 * The ops table and allocator are copied by value. */
cat89_status cat89_category_new(const cat89_category_ops *ops, void *ctx,
                                const cat89_allocator *allocator,
                                cat89_category **out_category);

/* Reference-counted category lifetime. Release no-ops on NULL. */
cat89_status cat89_category_retain(cat89_category *category);

void cat89_category_release(cat89_category *category);

/* Domain/codomain of a morphism. Returned object handles are borrowed. */
cat89_status cat89_dom(const cat89_category *category, const cat89_mor *mor,
                       const cat89_obj **out_obj);

cat89_status cat89_cod(const cat89_category *category, const cat89_mor *mor,
                       const cat89_obj **out_obj);

/* Identity on obj. Returns one owned morphism reference. */
cat89_status cat89_identity(const cat89_category *category,
                            const cat89_obj *obj, cat89_mor **out_mor);

/* Composition: out = g o f. Generic wrapper validates cod(f) vs dom(g) via
 * obj_same and returns CAT89_DOMAIN on mismatch without invoking backend
 * compose. Returns one owned morphism reference. */
cat89_status cat89_compose(const cat89_category *category, const cat89_mor *g,
                           const cat89_mor *f, cat89_mor **out_mor);

/* Structural object identity (borrowed, backend-defined). Returns nonzero iff
 * structurally the same object for typing purposes. */
int cat89_obj_same(const cat89_category *category, const cat89_obj *a,
                   const cat89_obj *b);

/* Morphism reference lifetime. retain requires ownership of mor by category;
 * release is a no-op for NULL or foreign handles. */
cat89_status cat89_mor_retain(const cat89_category *category, cat89_mor *mor);

void cat89_mor_release(const cat89_category *category, cat89_mor *mor);

/* Raw-handle provenance. Return exactly 0 or 1: 1 iff the handle is presently
 * valid for `category`. NULL category or handle yields 0. Safe on live foreign
 * handles (no dereference/inspect/retain/release of the candidate). */
int cat89_owns_obj(const cat89_category *category, const cat89_obj *obj);

int cat89_owns_mor(const cat89_category *category, const cat89_mor *mor);

#endif
