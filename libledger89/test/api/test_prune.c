/* test_prune.c - prefix pruning granularity and revision stability. */

#include "fixture.h"
#include "test.h"

static void fill(fx *f, unsigned long count)
{
    unsigned long i;

    for (i = 0ul; i < count; ++i)
    {
        unsigned char b;
        ledger89_slice s;

        b = (unsigned char)('a' + (int)(i % 26ul));
        s.data = &b;
        s.size = 1u;
        CHECK_EQ(ledger89_appendv(f->l, &s, 1u, NULL), LEDGER89_OK);
    }
}

static void test_whole_parts(void)
{
    fx f;
    ledger89_index actual;
    ledger89_state st;
    unsigned char buf[4];
    size_t size;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    fill(&f, 3u);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_rotate(f.l);
    CHECK_EQ(rc, LEDGER89_OK);
    fill(&f, 3u);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(fx_count_parts(&f), 2);

    /* requested inside the sealed part: nothing can be dropped. */
    actual = test_u64(99);
    rc = ledger89_prune_before(f.l, test_u64(3), &actual);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(actual, test_u64(1));

    /* requested at the sealed part boundary: the whole part goes. */
    rc = ledger89_prune_before(f.l, test_u64(4), &actual);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(actual, test_u64(4));
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.first, test_u64(4));
    CHECK_EQ(st.revision.lo, 0u);
    rc = ledger89_read(f.l, test_u64(1), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_EGONE);
    rc = ledger89_read(f.l, test_u64(4), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(fx_count_parts(&f), 1);
    fx_close(&f);
}

static void test_preconditions(void)
{
    fx f;
    ledger89_index actual;
    ledger89_slice s;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    fill(&f, 2u);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_rotate(f.l);
    CHECK_EQ(rc, LEDGER89_OK);

    /* requested > stable_end is unstable. */
    actual = test_u64(0);
    rc = ledger89_prune_before(f.l, test_u64(9), &actual);
    CHECK_EQ(rc, LEDGER89_EUNSTABLE);

    /* requested <= first is a no-op. */
    actual = test_u64(0);
    rc = ledger89_prune_before(f.l, test_u64(1), &actual);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(actual, test_u64(1));

    /* Pruning may run while the active tail is dirty. */
    s.data = "x";
    s.size = 1u;
    rc = ledger89_appendv(f.l, &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_prune_before(f.l, test_u64(3), &actual);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(actual, test_u64(3));
    rc = ledger89_prune_before(f.l, test_u64(3), &actual);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(actual, test_u64(3));
    rc = ledger89_prune_before(f.l, test_u64(3), NULL);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    fx_close(&f);
}

int main(void)
{
    test_whole_parts();
    test_preconditions();
    TEST_END;
}
