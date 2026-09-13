#ifndef CAT89_SPLIT_H
#define CAT89_SPLIT_H

#include <cat89/alloc.h>
#include <cat89/core.h>
#include <cat89/iso.h>

/* cat89_split.h - split (section/retraction) structures (CAT-I3).
 *
 * A split monomorphism is a section s : A -> B together with a retraction
 * r : B -> A promised to satisfy r o s = 1_A. A split epimorphism is the dual
 * (its retraction-then-section composes to an identity the other way). As with
 * isomorphisms these package ordinary morphisms; the law is a contract checked
 * by check.h, not by the constructor. */

typedef struct cat89_split_mono cat89_split_mono;
typedef struct cat89_split_epi cat89_split_epi;

/* Split mono: section : A -> B, retraction : B -> A. */
cat89_status cat89_split_mono_new(cat89_category *category, cat89_mor *section,
                                  cat89_mor *retraction,
                                  const cat89_allocator *allocator,
                                  cat89_split_mono **out_split);

cat89_category *cat89_split_mono_category(const cat89_split_mono *split);

const cat89_mor *cat89_split_mono_section(const cat89_split_mono *split);

const cat89_mor *cat89_split_mono_retraction(const cat89_split_mono *split);

cat89_status cat89_split_mono_retain(cat89_split_mono *split);

void cat89_split_mono_release(cat89_split_mono *split);

/* Identity split mono on an object: (1_A, 1_A). */
cat89_status cat89_split_mono_identity(cat89_category *category,
                                       const cat89_obj *obj,
                                       const cat89_allocator *allocator,
                                       cat89_split_mono **out_split);

/* Compose split monos: section = s2 o s1, retraction = r1 o r2. */
cat89_status cat89_split_mono_compose(const cat89_split_mono *g,
                                      const cat89_split_mono *f,
                                      const cat89_allocator *allocator,
                                      cat89_split_mono **out_split);

/* Split epi: section : A -> B, retraction : B -> A with the dual law. */
cat89_status cat89_split_epi_new(cat89_category *category, cat89_mor *section,
                                 cat89_mor *retraction,
                                 const cat89_allocator *allocator,
                                 cat89_split_epi **out_split);

cat89_category *cat89_split_epi_category(const cat89_split_epi *split);

const cat89_mor *cat89_split_epi_section(const cat89_split_epi *split);

const cat89_mor *cat89_split_epi_retraction(const cat89_split_epi *split);

cat89_status cat89_split_epi_retain(cat89_split_epi *split);

void cat89_split_epi_release(cat89_split_epi *split);

/* Identity split epi on an object: (1_A, 1_A). */
cat89_status cat89_split_epi_identity(cat89_category *category,
                                      const cat89_obj *obj,
                                      const cat89_allocator *allocator,
                                      cat89_split_epi **out_split);

/* Compose split epis: section = s2 o s1, retraction = r1 o r2. */
cat89_status cat89_split_epi_compose(const cat89_split_epi *g,
                                     const cat89_split_epi *f,
                                     const cat89_allocator *allocator,
                                     cat89_split_epi **out_split);

/* Package an isomorphism's two morphisms as a split mono or split epi. */
cat89_status cat89_iso_as_split_mono(const cat89_iso *iso,
                                     const cat89_allocator *allocator,
                                     cat89_split_mono **out_split);

cat89_status cat89_iso_as_split_epi(const cat89_iso *iso,
                                    const cat89_allocator *allocator,
                                    cat89_split_epi **out_split);

#endif
