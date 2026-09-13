/* test_dyn_incremental.c - J01..J07: the editor MUST NOT derive its transform
 * from a static BWT/SA-IS after it has opened. RED for the lazy-recompute
 * baseline; Salson incremental maintenance must make it green. */
#include <stddef.h>

#include "test.h"

#include <bwt89_internal.h>

static void query_without_recompute(void)
{
    struct bwt89_ed *ed;
    unsigned char tf[8192];
    size_t index;
    size_t i;
    enum bwt89_status st;
    ed = NULL;
    st = bwt89_ed_open(&ed, (const unsigned char *)"banana", 6);
    CHECK(st == BWT89_OK);
    bwt89_test_reset_stats();
    for (i = 0; i < 100; ++i)
    {
        st = bwt89_ed_bwt(ed, &index, tf);
        CHECK(st == BWT89_OK);
    }
    CHECK(bwt89_test_bwt_calls() == 0);
    CHECK(bwt89_test_sais_calls() == 0);
    bwt89_ed_close(ed);
}

static void edit_without_recompute(void)
{
    struct bwt89_ed *ed;
    unsigned char tf[8192];
    size_t index;
    int i;
    enum bwt89_status st;
    ed = NULL;
    st = bwt89_ed_open(&ed, (const unsigned char *)"banana", 6);
    CHECK(st == BWT89_OK);
    bwt89_test_reset_stats();
    for (i = 0; i < 1000; ++i)
    {
        st = bwt89_ed_insert(ed, 0, (unsigned char)('a' + (i % 3)));
        CHECK(st == BWT89_OK);
        st = bwt89_ed_bwt(ed, &index, tf);
        CHECK(st == BWT89_OK);
    }
    CHECK(bwt89_test_bwt_calls() == 0);
    CHECK(bwt89_test_sais_calls() == 0);
    bwt89_ed_close(ed);
}

int main(void)
{
    query_without_recompute();
    edit_without_recompute();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
