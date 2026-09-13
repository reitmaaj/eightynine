#ifndef BWT89_INTERNAL_H
#define BWT89_INTERNAL_H

#include <stddef.h>

/* Internal helpers shared across libbwt89 translation units. Not public;
 * declared here so each definition is preceded by a prototype (green
 * requires -Wmissing-prototypes). Unit tests may reach the test-control
 * helpers for oracle and fault-injection checks by including this header
 * with -Isrc.

 * The empty GREEN_PURE annotation lets green treat a following (genuinely
 * pure) function as callable in expression position.
 */
#define GREEN_PURE

/* Every allocation inside libbwt89 routes through these so test builds can
 * count live allocations and inject failures. In production they forward
 * directly to the C allocation functions. */
void *bwt89_malloc(size_t n);
void *bwt89_realloc(void *p, size_t n);
void bwt89_free(void *p);

/* Instrumentation accounting (no-op cost in production). bwt89_note_* are
 * called by the entry points they observe. */
void bwt89_note_bwt(void);
void bwt89_note_sais(void);
void bwt89_note_edit_insert(void);
void bwt89_note_edit_delete(void);
void bwt89_note_edit_substitute(void);

/* Test-control surface: statistics reset, deterministic allocation failure
 * injection, and query accessors. Not part of the public API. */
void bwt89_test_reset_stats(void);
void bwt89_test_fail_alloc_at(unsigned long k);
void bwt89_test_disable_alloc_failure(void);
unsigned long bwt89_test_live_allocs(void);
unsigned long bwt89_test_alloc_calls(void);
unsigned long bwt89_test_malloc_calls(void);
unsigned long bwt89_test_realloc_calls(void);
unsigned long bwt89_test_free_calls(void);
unsigned long bwt89_test_bwt_calls(void);
unsigned long bwt89_test_sais_calls(void);
unsigned long bwt89_test_ed_insert_steps(void);
unsigned long bwt89_test_ed_delete_steps(void);
unsigned long bwt89_test_ed_substitute_steps(void);

struct bwt89_ed;

/* Return nonzero when an editor handle is internally consistent (non-NULL
 * storage, logical length within capacity). Test-only helper. */
int bwt89_test_ed_check(const struct bwt89_ed *ed);

/* Contract: s[0..n-1] with n >= 1, s[n-1] == 0 is the unique minimum symbol,
 * every symbol is in [0, K]. Computes the suffix array of s into sa[0..n-1]
 * (sa[0] == n-1, the sentinel). Returns 0 on success, nonzero on allocation
 * failure. */
int bwt89_sa_is(const int *s, int *sa, int n, int K);

#endif /* BWT89_INTERNAL_H */
