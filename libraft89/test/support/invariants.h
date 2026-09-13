#ifndef INVARIANTS_H
#define INVARIANTS_H

/* invariants.h - test-only global Raft safety invariants I01..I20 over
 * a simulated cluster. Not part of the library. */

#include "raft_cluster.h"

/* Check invariants that depend only on the current cluster state.
 * Returns NULL when satisfied, or a static description of the first
 * violation. */
const char *invariants_check(const cluster *c);

/* Check invariants that compare a transition against its parent state:
 * I02, I03, I06, I07, I11, I12, I13, and I19/I20. */
const char *invariants_check_transition(const cluster *prev,
                                        const cluster *next);

#endif /* INVARIANTS_H */
