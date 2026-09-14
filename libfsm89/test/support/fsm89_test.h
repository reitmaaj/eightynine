#ifndef FSM89_TEST_H
#define FSM89_TEST_H

#include <stddef.h>

#include "fsm89.h"

#define FSM89_TEST_MAX_EFFECTS 64

extern int fsm89_test_failures;
extern int fsm89_test_checks;

void fsm89_test_check(int cond, const char *what);
void fsm89_test_check_status(int got, int want, const char *what);
void fsm89_test_check_ul(unsigned long got, unsigned long want,
                         const char *what);
void fsm89_test_check_size(size_t got, size_t want, const char *what);

/* Span identity: pointer and count both match. For an empty span only the
 * count is compared because v may be NULL or non-NULL. */
void fsm89_test_check_span(fsm89_effects got, const fsm89_effect *v, size_t n,
                           const char *what);

/* Span contents: the first n effects equal want. */
void fsm89_test_check_span_eq(fsm89_effects got, const fsm89_effect *v,
                              size_t n, const char *what);

/* 1 when both spans have the same count and equal contents. */
int fsm89_test_span_equal(fsm89_effects a, fsm89_effects b);

/* Field-by-field snapshot of a step result. Padding bytes are never
 * compared. */
typedef struct fsm89_test_snapshot
{
    fsm89_state from;
    fsm89_event event;
    fsm89_state to;
    const fsm89_effect *leave_v;
    size_t leave_n;
    const fsm89_effect *edge_v;
    size_t edge_n;
    const fsm89_effect *enter_v;
    size_t enter_n;
} fsm89_test_snapshot;

void fsm89_test_snapshot_take(fsm89_test_snapshot *out,
                              const fsm89_step_result *r);
void fsm89_test_unchanged(const fsm89_step_result *r,
                          const fsm89_test_snapshot *before, const char *what);

/* Flatten leave || edge || enter into out; returns the total effect count.
 * At most cap effects are stored. */
size_t fsm89_test_flatten(const fsm89_step_result *r, fsm89_effect *out,
                          size_t cap);

/* Flatten and compare against the exact expected sequence. */
void fsm89_test_expect_seq(const fsm89_step_result *r, const fsm89_effect *want,
                           size_t n, const char *what);

/* Deterministic xorshift PRNG for generated tests. */
unsigned long fsm89_test_rand(unsigned long *state);

int fsm89_test_report(void);

#endif
