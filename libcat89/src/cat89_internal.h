#ifndef CAT89_INTERNAL_H
#define CAT89_INTERNAL_H

/* cat89_internal.h - private shared helpers. Not installed. */

#include <cat89/alloc.h>
#include <cat89/core.h>
#include <cat89/finite.h>
#include <cat89/status.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

/* Reference-count helpers. inc returns CAT89_INVALID on overflow; dec returns
 * nonzero when the count drops to zero (caller should destroy). */
cat89_status cat89_ref_inc(unsigned long *refs);

int cat89_ref_dec(unsigned long *refs);

/* Checked size arithmetic; CAT89_NOMEM on overflow/wrap. */
cat89_status cat89_size_add(size_t a, size_t b, size_t *out);

cat89_status cat89_size_mul(size_t a, size_t b, size_t *out);

/* Checked growing vector of fixed-size elements. Growth allocates a new
 * buffer, copies and frees the old one, so a failed push leaves the vector
 * unchanged. The caller supplies the allocator on every mutating call. */
typedef struct cat89_vec
{
    void *data;
    size_t len;
    size_t cap;
    size_t elem;
} cat89_vec;

void cat89_vec_init(cat89_vec *vec, size_t elem);

cat89_status cat89_vec_push(cat89_vec *vec, const cat89_allocator *allocator,
                            const void *elem);

void *cat89_vec_at(const cat89_vec *vec, size_t i);

void cat89_vec_free(cat89_vec *vec, const cat89_allocator *allocator);

/* Allocate through an allocator, never passing zero (normalized to 1 byte).
 * Returns NULL on failure. */
void *cat89_alloc(const cat89_allocator *allocator, size_t size);

void *cat89_realloc(const cat89_allocator *allocator, void *ptr, size_t size);

void cat89_free(const cat89_allocator *allocator, void *ptr);

/* Backend-private: return the category's ctx (for concrete-backend helpers). */
void *cat89_category_ctx(const cat89_category *category);

/* Test-only hooks (white-box overflow/validation tests). Not installed. */
void cat89_category_test_set_refs(cat89_category *category, unsigned long refs);

void cat89_finite_test_set_mor_refs(cat89_mor *mor, unsigned long refs);

/* Test-only: build a finite table without validation (malformed-law fixtures).
 * The public cat89_finite_build always validates. */
cat89_status cat89_finite_build_unchecked(cat89_finite_builder *builder,
                                          cat89_category **out_category,
                                          cat89_eq **out_eq,
                                          cat89_enum **out_enum);

#endif
