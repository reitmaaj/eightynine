#ifndef CAT89_FINDLIM_H
#define CAT89_FINDLIM_H

#include <cat89/alloc.h>
#include <cat89/core.h>
#include <cat89/enum.h>
#include <cat89/eq.h>
#include <cat89/limit.h>

/* cat89_findlim.h - finite universal-construction finders (binary product).
 *
 * Given a finite ambient category with an enumeration capability and an
 * explicit `cat89_eq`, find the object carrying a universal property and
 * return it as a genuine `cat89_limit` whose limiting cone carries a working
 * universal `factor` (CAT-I9). Detection needs decidable morphism equality
 * (uniqueness of the mediating arrow), so every finder takes an explicit
 * `cat89_eq`; there is no silent inference.
 *
 * The returned limit is a limit of the DISCRETE2 shape diagram mapping its
 * two points to `a` and `b`. The two shape points are handed back through the
 * optional `*out_point_a` / `*out_point_b` out-parameters (borrowed; owned by
 * the diagram's source category, valid for the limit's lifetime) so a caller
 * can build candidate cones over the returned limit's diagram and invoke the
 * generic `cat89_limit_factor`. Either may be NULL to skip.
 *
 * On absence the finder returns CAT89_NOT_FOUND and leaves *out == NULL; on an
 * unusable argument it returns CAT89_INVALID and leaves *out == NULL.
 * Object identity used internally is structural (stable finite handles). */

/* Find the binary product a x b in `category` (enumerated by `enumeration`)
 * and return a limit of the DISCRETE2 diagram mapping point a -> a and
 * point b -> b. One owned limit result; caller releases via
 * cat89_limit_release. */
cat89_status cat89_find_binary_product(cat89_category *category,
                                       cat89_enum *enumeration, cat89_eq *eq,
                                       const cat89_obj *a, const cat89_obj *b,
                                       const cat89_allocator *allocator,
                                       cat89_limit **out_limit,
                                       const cat89_obj **out_point_a,
                                       const cat89_obj **out_point_b);

/* Dual: find the binary coproduct a + b and return a colimit of the DISCRETE2
 * diagram mapping point a -> a and point b -> b. One owned colimit result;
 * caller releases via cat89_colimit_release. */
cat89_status cat89_find_binary_coproduct(cat89_category *category,
                                         cat89_enum *enumeration, cat89_eq *eq,
                                         const cat89_obj *a, const cat89_obj *b,
                                         const cat89_allocator *allocator,
                                         cat89_colimit **out_colimit,
                                         const cat89_obj **out_point_a,
                                         const cat89_obj **out_point_b);

/* Find the equalizer of parallel arrows f, g : X -> Y and return a limit of
 * the PARALLEL diagram. One owned limit result; caller releases via
 * cat89_limit_release. */
cat89_status cat89_find_equalizer(cat89_category *category,
                                  cat89_enum *enumeration, cat89_eq *eq,
                                  cat89_mor *f, cat89_mor *g,
                                  const cat89_allocator *allocator,
                                  cat89_limit **out_limit,
                                  const cat89_obj **out_point_a,
                                  const cat89_obj **out_point_b);

/* Dual: find the coequalizer of parallel arrows f, g : X -> Y and return a
 * colimit of the PARALLEL diagram. One owned colimit result; caller releases
 * via cat89_colimit_release. */
cat89_status cat89_find_coequalizer(cat89_category *category,
                                    cat89_enum *enumeration, cat89_eq *eq,
                                    cat89_mor *f, cat89_mor *g,
                                    const cat89_allocator *allocator,
                                    cat89_colimit **out_colimit,
                                    const cat89_obj **out_point_a,
                                    const cat89_obj **out_point_b);

/* Find the pullback of arrows f : X -> Z and g : Y -> Z (shared codomain) and
 * return a limit of the SPAN diagram. The three diagram shape points are
 * handed back through the optional out_pt0/out_pt1/out_pt2 parameters
 * (mapped to X, Y, Z). One owned limit result; caller releases via
 * cat89_limit_release. */
cat89_status
cat89_find_pullback(cat89_category *category, cat89_enum *enumeration,
                    cat89_eq *eq, cat89_mor *f, cat89_mor *g,
                    const cat89_allocator *allocator, cat89_limit **out_limit,
                    const cat89_obj **out_pt0, const cat89_obj **out_pt1,
                    const cat89_obj **out_pt2);

/* Dual: find the pushout of arrows f : X -> Y and g : X -> Z (shared domain)
 * and return a colimit of the COSPAN diagram. The three diagram shape points
 * are handed back through the optional out_pt0/out_pt1/out_pt2 parameters.
 * One owned colimit result; caller releases via cat89_colimit_release. */
cat89_status
cat89_find_pushout(cat89_category *category, cat89_enum *enumeration,
                   cat89_eq *eq, cat89_mor *f, cat89_mor *g,
                   const cat89_allocator *allocator,
                   cat89_colimit **out_colimit, const cat89_obj **out_pt0,
                   const cat89_obj **out_pt1, const cat89_obj **out_pt2);

#endif
