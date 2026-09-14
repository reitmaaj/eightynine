#ifndef SYNTAX89_FAULT_ALLOC_H
#define SYNTAX89_FAULT_ALLOC_H

#include "syntax89.h"

/* Deterministic allocation-failure injection. Fails the Nth allocation
 * request and counts live blocks for leak detection. */

struct syntax89_fault_alloc
{
    unsigned long calls;   /* allocator invocations so far */
    unsigned long fail_at; /* fail call number fail_at; 0 disables */
    unsigned long allocs;  /* successful allocations */
    unsigned long frees;   /* successful frees of non-NULL blocks */
    unsigned long live;    /* live blocks */
};

void syntax89_fault_alloc_init(struct syntax89_fault_alloc *f);

/* Populate an allocator forwarding to malloc/realloc/free. */
void syntax89_fault_alloc_use(struct syntax89_fault_alloc *f,
                              syntax89_allocator *out);

/* Fail allocation number n (1-based); resets the call counter. 0 disables. */
void syntax89_fault_alloc_fail_at(struct syntax89_fault_alloc *f,
                                  unsigned long n);

void syntax89_fault_alloc_disable(struct syntax89_fault_alloc *f);

#endif
