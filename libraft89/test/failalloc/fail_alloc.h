#ifndef FAIL_ALLOC_H
#define FAIL_ALLOC_H

/* fail_alloc.h - allocation-failure injection used with the GNU ld
 * --wrap option. Test-only; not part of the library. */

#define FAIL_ALLOC_NEVER 0xFFFFFFFFul

/* Reset the allocation counter and arm failure at the given zero-based
 * allocation index. The live count is preserved. */
void fail_alloc_begin(unsigned long fail_index);

/* Disarm failure injection. */
void fail_alloc_disable(void);

/* Number of allocations that have not been freed yet. */
unsigned long fail_alloc_live(void);

#endif /* FAIL_ALLOC_H */
