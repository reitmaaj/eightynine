#ifndef CAT89_SHAPE_H
#define CAT89_SHAPE_H

#include <cat89/alloc.h>
#include <cat89/core.h>
#include <cat89/enum.h>
#include <cat89/eq.h>

/* cat89_shape.h - small reusable finite diagram shape categories.
 *
 * Diagrams are functors out of a shape category (the comma/diagram layer uses
 * `cat89_functor` as `cat89_diagram`). These builders construct the canonical
 * finite shapes used to express universal constructions and exhaustive checks,
 * returning an ordinary finite `cat89_category` with its equality and
 * enumeration capabilities. Shapes:
 *
 *   CAT89_SHAPE_EMPTY      - no objects (limit = terminal, colimit = initial).
 *   CAT89_SHAPE_DISCRETE2  - two objects, only identities (binary product /
 *                            coproduct).
 *   CAT89_SHAPE_PARALLEL   - X -> Y with two parallel arrows f, g (equalizer /
 *                            coequalizer).
 *   CAT89_SHAPE_SPAN       - two arrows into a shared object (0->2, 1->2); a
 *                            limit of a diagram over it is a pullback.
 *   CAT89_SHAPE_COSPAN     - two arrows out of a shared object (0->1, 0->2); a
 *                            colimit of a diagram over it is a pushout.
 */

enum cat89_shape_kind
{
    CAT89_SHAPE_EMPTY = 0,
    CAT89_SHAPE_DISCRETE2,
    CAT89_SHAPE_PARALLEL,
    CAT89_SHAPE_SPAN,
    CAT89_SHAPE_COSPAN
};

/* Build the named shape category plus its eq and enum capabilities. Any of
 * out_eq / out_enum may be NULL to skip. */
cat89_status cat89_shape_category_new(enum cat89_shape_kind kind,
                                      const cat89_allocator *allocator,
                                      cat89_category **out_category,
                                      cat89_eq **out_eq, cat89_enum **out_enum);

#endif
