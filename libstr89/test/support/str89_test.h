#ifndef STR89_TEST_H
#define STR89_TEST_H

#include <stddef.h>

#include "str89.h"

extern int str89_test_failures;
extern int str89_test_checks;

void str89_test_check(int cond, const char *what);
int str89_test_report(void);

/* Invariant oracle: shape plus libu89 validation. */
void str89_test_valid_view(str89_view v, const char *what);
void str89_test_valid_str(const str89 *s, const char *what);
void str89_test_valid_buf(const str89_buf *s, const char *what);

/* Check that v holds exactly [bytes, bytes + len). */
void str89_test_view_is(str89_view v, const unsigned char *bytes, size_t len,
                        const char *what);

/* ---- Deterministic fault allocator -------------------------------------- */

#define STR89_TEST_SLOTS 64

typedef struct str89_test_slot
{
    void *p;
    size_t size;
    int used;
} str89_test_slot;

typedef struct str89_test_fault
{
    str89_alloc alloc;
    long fail_at; /* 1-based allocation index to fail; 0 = never */
    long calls;   /* allocation attempts (malloc + realloc) */
    long frees;   /* free calls on live blocks */
    long live;    /* live blocks */
    long bad_free; /* unknown, double, or cross-allocator free observed */
    unsigned long tag;
    str89_test_slot slots[STR89_TEST_SLOTS];
} str89_test_fault;

void str89_test_fault_init(str89_test_fault *f, unsigned long tag);

/* Fail the n-th allocation after this call; n == 0 disarms. */
void str89_test_fault_fail_next(str89_test_fault *f, long n);

int str89_test_fault_leaked(const str89_test_fault *f);

/* ---- Deterministic PRNG -------------------------------------------------- */

unsigned long str89_test_rand(unsigned long *state);

#endif
