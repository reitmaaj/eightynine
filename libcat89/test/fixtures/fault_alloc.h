#ifndef CAT89_FAULT_ALLOC_H
#define CAT89_FAULT_ALLOC_H

/* cat89_fault_alloc.h - allocation-failure injection fixture (F07). */

#include <cat89/alloc.h>

struct cat89_fault_alloc
{
    unsigned long count;
    unsigned long fail_at;
    unsigned long allocs;
    unsigned long frees;
};

void cat89_fault_alloc_init(struct cat89_fault_alloc *f);

/* Populate an allocator forwarding to malloc/free, failing the Nth call. */
void cat89_fault_alloc_use(struct cat89_fault_alloc *f, cat89_allocator *out);

/* Fail allocation number `n` (1-based); 0 disables failure. */
void cat89_fault_alloc_fail_at(struct cat89_fault_alloc *f, unsigned long n);

void cat89_fault_alloc_disable(struct cat89_fault_alloc *f);

#endif
