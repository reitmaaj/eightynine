#ifndef STR89_INTERNAL_H
#define STR89_INTERNAL_H

#include <stddef.h>

#include "str89.h"

/* Resolved allocator calls; alloc == NULL selects the C library. */
void *str89__malloc(const str89_alloc *alloc, size_t size);
void *str89__realloc(const str89_alloc *alloc, void *ptr, size_t size);
void str89__free(const str89_alloc *alloc, void *ptr);

/* Checked addition. Returns STR89_OK or STR89_ERANGE. */
int str89__add(size_t a, size_t b, size_t *out);

/* Next capacity for a growth from cap to need; 0 when doubling would
 * overflow size_t. */
size_t str89__grow_cap(size_t cap, size_t need);

/* 1 when src lies in [base, base + len). */
int str89__overlaps(const unsigned char *base, size_t len,
                    const unsigned char *src);

/* 1 when data/len is a representable empty-or-valid pair. */
int str89__data_ok(const unsigned char *data, size_t len);

#endif
