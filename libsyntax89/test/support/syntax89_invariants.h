#ifndef SYNTAX89_INVARIANTS_H
#define SYNTAX89_INVARIANTS_H

#include "syntax89.h"

/* White-box consistency checker and observable snapshot harness. Both are
 * test-only; the library exposes no debug surface. */

/* Assert every structural invariant of g. Call after each successful
 * mutator in tests. */
void syntax89_test_check(const syntax89_graph *g);

struct syntax89_test_snapshot;

/* Copy the full observable state of g. Returns 0 on success, -1 when out of
 * memory. */
int syntax89_test_snapshot_take(const syntax89_graph *g,
                                struct syntax89_test_snapshot **out);

/* 1 when the snapshot equals g's current observable state. */
int syntax89_test_snapshot_equal(const struct syntax89_test_snapshot *s,
                                 const syntax89_graph *g);

void syntax89_test_snapshot_free(struct syntax89_test_snapshot *s);

#endif
