#ifndef CAT89_CONE_H
#define CAT89_CONE_H

#include <cat89/alloc.h>
#include <cat89/core.h>
#include <cat89/diagram.h>

/* cat89_cone.h - cones and cocones over a diagram D : J -> C.
 *
 * A cone consists of an apex X (an object of C) and, for every object j of the
 * shape J, a leg X -> D(j) (a morphism of C). A cocone is the dual: legs
 * D(j) -> X. The apex is borrowed from the ambient category C (the functor
 * target of the diagram); cone legs are returned as owned morphisms in C.
 * Commutativity is a provider contract; check.h supplies check_cone/cocone. */

typedef struct cat89_cone cat89_cone;
typedef struct cat89_cocone cat89_cocone;

typedef struct cat89_cone_ops
{
    cat89_status (*leg)(void *ctx, const cat89_obj *shape_obj,
                        cat89_mor **out_mor);

    void (*destroy)(void *ctx);
} cat89_cone_ops;

/* Construct a cone over `diagram` with the given borrowed apex. Retains the
 * diagram. ops must be non-null with a leg callback. */
cat89_status cat89_cone_new(cat89_diagram *diagram, const cat89_obj *apex,
                            const cat89_cone_ops *ops, void *ctx,
                            const cat89_allocator *allocator,
                            cat89_cone **out_cone);

/* One owned leg morphism apex -> D(j) in the ambient category. */
cat89_status cat89_cone_leg(const cat89_cone *cone, const cat89_obj *shape_obj,
                            cat89_mor **out_mor);

cat89_diagram *cat89_cone_diagram(const cat89_cone *cone);

const cat89_obj *cat89_cone_apex(const cat89_cone *cone);

cat89_category *cat89_cone_category(const cat89_cone *cone);

cat89_status cat89_cone_retain(cat89_cone *cone);

void cat89_cone_release(cat89_cone *cone);

/* Cocone: legs D(j) -> apex. */
cat89_status cat89_cocone_new(cat89_diagram *diagram, const cat89_obj *apex,
                              const cat89_cone_ops *ops, void *ctx,
                              const cat89_allocator *allocator,
                              cat89_cocone **out_cocone);

cat89_status cat89_cocone_leg(const cat89_cocone *cocone,
                              const cat89_obj *shape_obj, cat89_mor **out_mor);

cat89_diagram *cat89_cocone_diagram(const cat89_cocone *cocone);

const cat89_obj *cat89_cocone_apex(const cat89_cocone *cocone);

cat89_category *cat89_cocone_category(const cat89_cocone *cocone);

cat89_status cat89_cocone_retain(cat89_cocone *cocone);

void cat89_cocone_release(cat89_cocone *cocone);

#endif
