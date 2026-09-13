/* bwt89_alloc.c - allocation and instrumentation backend.
 *
 * Every allocation inside libbwt89 routes through bwt89_malloc / bwt89_realloc
 * / bwt89_free so that the test surface can count live allocations and inject
 * deterministic allocation failures (fail the kth attempt). In production
 * these forward to the C allocation functions with negligible accounting
 * overhead and no behavioral difference.
 *
 * The note_* entry points let the modules report static-transform and edit
 * activity so the dynamic module's "no full recompute" claim can be checked
 * mechanically rather than by inspection. */

#include <stdlib.h>

#include "bwt89_internal.h"

static unsigned long g_malloc_calls;
static unsigned long g_realloc_calls;
static unsigned long g_free_calls;
static unsigned long g_live_allocs;
static unsigned long g_alloc_ord;
static unsigned long g_fail_at;
static unsigned long g_bwt_calls;
static unsigned long g_sais_calls;
static unsigned long g_ed_insert;
static unsigned long g_ed_delete;
static unsigned long g_ed_substitute;

/* Return nonzero if this allocation attempt must fail. */
static int should_fail(void)
{
    if (g_fail_at == 0)
    {
        return 0;
    }
    ++g_alloc_ord;
    if (g_alloc_ord == g_fail_at)
    {
        return 1;
    }
    return 0;
}

void *bwt89_malloc(size_t n)
{
    void *p;
    int fail;
    ++g_malloc_calls;
    fail = should_fail();
    if (fail != 0)
    {
        return NULL;
    }
    p = malloc(n);
    if (p != NULL)
    {
        ++g_live_allocs;
    }
    return p;
}

void *bwt89_realloc(void *p, size_t n)
{
    void *q;
    int fail;
    ++g_realloc_calls;
    fail = should_fail();
    if (fail != 0)
    {
        return NULL;
    }
    q = realloc(p, n);
    if (q != NULL)
    {
        if (p == NULL)
        {
            ++g_live_allocs;
        }
    }
    return q;
}

void bwt89_free(void *p)
{
    ++g_free_calls;
    if (p != NULL)
    {
        --g_live_allocs;
    }
    free(p);
}

void bwt89_note_bwt(void)
{
    ++g_bwt_calls;
}

void bwt89_note_sais(void)
{
    ++g_sais_calls;
}

void bwt89_note_edit_insert(void)
{
    ++g_ed_insert;
}

void bwt89_note_edit_delete(void)
{
    ++g_ed_delete;
}

void bwt89_note_edit_substitute(void)
{
    ++g_ed_substitute;
}

void bwt89_test_reset_stats(void)
{
    g_malloc_calls = 0;
    g_realloc_calls = 0;
    g_free_calls = 0;
    g_live_allocs = 0;
    g_alloc_ord = 0;
    g_fail_at = 0;
    g_bwt_calls = 0;
    g_sais_calls = 0;
    g_ed_insert = 0;
    g_ed_delete = 0;
    g_ed_substitute = 0;
}

void bwt89_test_fail_alloc_at(unsigned long k)
{
    g_alloc_ord = 0;
    g_fail_at = k;
}

void bwt89_test_disable_alloc_failure(void)
{
    g_fail_at = 0;
}

unsigned long bwt89_test_live_allocs(void)
{
    return g_live_allocs;
}

unsigned long bwt89_test_alloc_calls(void)
{
    return g_malloc_calls + g_realloc_calls;
}

unsigned long bwt89_test_malloc_calls(void)
{
    return g_malloc_calls;
}

unsigned long bwt89_test_realloc_calls(void)
{
    return g_realloc_calls;
}

unsigned long bwt89_test_free_calls(void)
{
    return g_free_calls;
}

unsigned long bwt89_test_bwt_calls(void)
{
    return g_bwt_calls;
}

unsigned long bwt89_test_sais_calls(void)
{
    return g_sais_calls;
}

unsigned long bwt89_test_ed_insert_steps(void)
{
    return g_ed_insert;
}

unsigned long bwt89_test_ed_delete_steps(void)
{
    return g_ed_delete;
}

unsigned long bwt89_test_ed_substitute_steps(void)
{
    return g_ed_substitute;
}
