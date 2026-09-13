/* test_dyn_robust.c - dynamic editor hardening: allocations route through the
 * library shim (leak-free close), edit/query counters, geometric growth, and
 * allocation-failure atomicity (acceptance H06-H10, D7). */
#include <stddef.h>

#include "test.h"

#include <bwt89_internal.h>

static void test_close_releases(void)
{
    struct bwt89_ed *ed;
    unsigned long before;
    unsigned char c;
    enum bwt89_status st;
    ed = NULL;
    bwt89_test_reset_stats();
    c = 'a';
    st = bwt89_ed_open(&ed, &c, 1);
    CHECK(st == BWT89_OK);
    CHECK(ed != NULL);
    CHECK(bwt89_test_ed_check(ed) != 0);
    before = bwt89_test_live_allocs();
    CHECK(before > 0);
    st = bwt89_ed_close(ed);
    CHECK(st == BWT89_OK);
    CHECK(bwt89_test_live_allocs() == 0);
    CHECK(bwt89_ed_close(NULL) == BWT89_NULL_ARG);
}

static void test_counters(void)
{
    struct bwt89_ed *ed;
    unsigned char c;
    unsigned long i0;
    unsigned long d0;
    unsigned long s0;
    enum bwt89_status st;
    ed = NULL;
    c = 'x';
    st = bwt89_ed_open(&ed, &c, 1);
    CHECK(st == BWT89_OK);
    i0 = bwt89_test_ed_insert_steps();
    d0 = bwt89_test_ed_delete_steps();
    s0 = bwt89_test_ed_substitute_steps();
    CHECK(bwt89_ed_insert(ed, 1, 'a') == BWT89_OK);
    CHECK(bwt89_ed_delete(ed, 0) == BWT89_OK);
    CHECK(bwt89_ed_substitute(ed, 0, 'z') == BWT89_OK);
    CHECK(bwt89_test_ed_insert_steps() == i0 + 1);
    CHECK(bwt89_test_ed_delete_steps() == d0 + 1);
    CHECK(bwt89_test_ed_substitute_steps() == s0 + 1);
    CHECK(bwt89_test_ed_check(ed) != 0);
    bwt89_ed_close(ed);
}

static void test_geometric_growth(void)
{
    struct bwt89_ed *ed;
    unsigned long rc0;
    unsigned char dummy;
    size_t len;
    int i;
    enum bwt89_status st;
    ed = NULL;
    dummy = 0;
    st = bwt89_ed_open(&ed, &dummy, 0);
    CHECK(st == BWT89_OK);
    rc0 = bwt89_test_realloc_calls();
    for (i = 0; i < 600; ++i)
    {
        if (bwt89_ed_insert(ed, 0, (unsigned char)'a') != BWT89_OK)
        {
            break;
        }
    }
    len = 0;
    CHECK(bwt89_ed_length(ed, &len) == BWT89_OK);
    CHECK(len == 600);
    /* geometric growth: reallocations are logarithmic, not once per insert */
    CHECK(bwt89_test_realloc_calls() - rc0 < 30);
    CHECK(bwt89_test_ed_check(ed) != 0);
    bwt89_ed_close(ed);
    CHECK(bwt89_test_live_allocs() == 0);
}

static void test_insert_atomic(void)
{
    struct bwt89_ed *ed;
    unsigned char c;
    size_t len0;
    size_t len1;
    enum bwt89_status st;
    ed = NULL;
    c = 'a';
    st = bwt89_ed_open(&ed, &c, 0);
    CHECK(st == BWT89_OK);
    /* prime one byte so len == cap == 1; the next insert must grow */
    CHECK(bwt89_ed_insert(ed, 0, 'a') == BWT89_OK);
    len0 = 0;
    CHECK(bwt89_ed_length(ed, &len0) == BWT89_OK);
    bwt89_test_fail_alloc_at(1);
    st = bwt89_ed_insert(ed, 1, 'b');
    CHECK(st == BWT89_NOMEM);
    len1 = 0;
    CHECK(bwt89_ed_length(ed, &len1) == BWT89_OK);
    CHECK(len1 == len0);
    CHECK(bwt89_test_ed_check(ed) != 0);
    bwt89_test_disable_alloc_failure();
    /* the same insert now succeeds */
    st = bwt89_ed_insert(ed, 1, 'b');
    CHECK(st == BWT89_OK);
    len1 = 0;
    CHECK(bwt89_ed_length(ed, &len1) == BWT89_OK);
    CHECK(len1 == len0 + 1);
    bwt89_ed_close(ed);
    CHECK(bwt89_test_live_allocs() == 0);
}

static void test_open_too_large(void)
{
    struct bwt89_ed *ed;
    unsigned char c;
    enum bwt89_status st;
    ed = NULL;
    c = 'a';
    st = bwt89_ed_open(&ed, &c, BWT89_MAX_N + 1);
    CHECK(st == BWT89_TOO_LARGE);
    CHECK(ed == NULL);
}

int main(void)
{
    test_close_releases();
    test_counters();
    test_geometric_growth();
    test_insert_atomic();
    test_open_too_large();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
