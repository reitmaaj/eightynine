#ifndef CAT89_CHECK_H
#define CAT89_CHECK_H

#include <cat89/cone.h>
#include <cat89/core.h>
#include <cat89/enum.h>
#include <cat89/eq.h>
#include <cat89/functor.h>
#include <cat89/iso.h>
#include <cat89/nat.h>
#include <cat89/split.h>

/* cat89_check.h - explicit law checking.
 *
 * Laws are contracts promised by backends; checking is a separate activity.
 * Every checker explicitly requires the capabilities (equality, enumeration)
 * its algorithm needs and never silently infers them. An unavailable required
 * capability yields CAT89_NOT_SUPPORTED rather than a false result. */

typedef struct cat89_check_result
{
    unsigned long checked;
    unsigned long failed;
    cat89_status status;
} cat89_check_result;

/* Local (witness) checks: verify one case against the supplied equality. */

cat89_status cat89_check_left_identity(cat89_category *category, cat89_eq *eq,
                                       const cat89_mor *f, int *out_valid);

cat89_status cat89_check_right_identity(cat89_category *category, cat89_eq *eq,
                                        const cat89_mor *f, int *out_valid);

/* Associativity for the composable triple f,g,h: (h o g) o f == h o (g o f). */
cat89_status cat89_check_associativity(cat89_category *category, cat89_eq *eq,
                                       const cat89_mor *h, const cat89_mor *g,
                                       const cat89_mor *f, int *out_valid);

/* Iso: verifies g o f = 1 and f o g = 1 for (forward, inverse). */
cat89_status cat89_check_iso(const cat89_iso *iso, cat89_eq *eq,
                             int *out_valid);

/* Split mono: retraction o section = identity. */
cat89_status cat89_check_split_mono(const cat89_split_mono *split, cat89_eq *eq,
                                    int *out_valid);

/* Split epi: dual identity law. */
cat89_status cat89_check_split_epi(const cat89_split_epi *split, cat89_eq *eq,
                                   int *out_valid);

/* Functor: F(1_A) == 1_{F(A)}. target_eq belongs to the functor target. */
cat89_status cat89_check_functor_identity(const cat89_functor *functor,
                                          cat89_eq *target_eq,
                                          const cat89_obj *obj, int *out_valid);

/* Functor: F(g o f) == F(g) o F(f) (g, f composable in the source). */
cat89_status cat89_check_functor_composition(const cat89_functor *functor,
                                             cat89_eq *target_eq, cat89_mor *g,
                                             cat89_mor *f, int *out_valid);

/* Naturality of eta : F => G for f : A -> B in the source:
 * G(f) o eta_A == eta_B o F(f). */
cat89_status cat89_check_naturality(const cat89_nat *nat, cat89_eq *target_eq,
                                    cat89_mor *f, int *out_valid);

/* Cone commutativity: for every shape morphism m : j -> k,
 * D(m) o leg_j == leg_k. eq belongs to the ambient category; shape_enum
 * enumerates the shape-category morphisms. */
cat89_status cat89_check_cone(const cat89_cone *cone, cat89_eq *eq,
                              cat89_enum *shape_enum, int *out_valid);

/* Cocone commutativity: for every shape morphism m : j -> k,
 * leg_j == leg_k o D(m). */
cat89_status cat89_check_cocone(const cat89_cocone *cocone, cat89_eq *eq,
                                cat89_enum *shape_enum, int *out_valid);

/* Exhaustively verify the category laws over an enumerated finite ambient:
 * left/right identity on every morphism and associativity on every composable
 * triple, counting each law case into `result`. Requires both `eq` and an
 * enumeration of the ambient's morphisms; if either capability is missing it
 * returns CAT89_NOT_SUPPORTED (never silent success). */
cat89_status cat89_check_category_exhaustive(cat89_category *category,
                                             cat89_eq *eq,
                                             cat89_enum *enumeration,
                                             cat89_check_result *result);

/* Exhaustive derived law-family sweeps over an enumerated finite ambient (W5).
 * Each returns CAT89_NOT_SUPPORTED when a required capability is absent,
 * never a silent partial pass. Every family counts each verified law case
 * into `checked` (iso counts both inverse equations per pair) and reports
 * genuinely failing law cases into `failed`. */

/* Count every isomorph pair (f, g) present in the ambient (2 cases each). */
cat89_status cat89_check_iso_exhaustive(cat89_category *category, cat89_eq *eq,
                                        cat89_enum *enumeration,
                                        cat89_check_result *result);

/* Count every split-mono / split-epi (section, retraction) pair present. */
cat89_status cat89_check_split_mono_exhaustive(cat89_category *category,
                                               cat89_eq *eq,
                                               cat89_enum *enumeration,
                                               cat89_check_result *result);

cat89_status cat89_check_split_epi_exhaustive(cat89_category *category,
                                              cat89_eq *eq,
                                              cat89_enum *enumeration,
                                              cat89_check_result *result);

/* Given a functor F and an enumeration of its source category, verify and
 * count F(1_A) == 1_{F(A)} over every source object and F(g o f) == F(g) o
 * F(f) over every composable source pair. target_eq belongs to F's target. */
cat89_status cat89_check_functor_exhaustive(cat89_functor *functor,
                                            cat89_eq *target_eq,
                                            cat89_enum *source_enum,
                                            cat89_check_result *result);

/* Given a natural transformation and an enumeration of its source category's
 * morphisms, verify and count the naturality square over every source morphism.
 */
cat89_status cat89_check_naturality_exhaustive(cat89_nat *nat,
                                               cat89_eq *target_eq,
                                               cat89_enum *source_enum,
                                               cat89_check_result *result);

/* Given a cone/cocone over a shape diagram and an enumeration of the shape
 * category's morphisms, verify and count every commutation cell. eq belongs to
 * the ambient category. */
cat89_status cat89_check_cone_exhaustive(const cat89_cone *cone, cat89_eq *eq,
                                         cat89_enum *shape_enum,
                                         cat89_check_result *result);

cat89_status cat89_check_cocone_exhaustive(const cat89_cocone *cocone,
                                           cat89_eq *eq, cat89_enum *shape_enum,
                                           cat89_check_result *result);

#endif
