/* test_truncate.c - suffix truncation semantics. */

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

static void test_preconditions(void)
{
    fx f;
    ledger89_state st;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    fill(&f, 3u);
    rc = ledger89_truncate_from(f.l, test_u64(2));
    CHECK_EQ(rc, LEDGER89_EUNSTABLE);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_truncate_from(f.l, test_u64(0));
    CHECK_EQ(rc, LEDGER89_EGONE);
    rc = ledger89_truncate_from(f.l, test_u64(9));
    CHECK_EQ(rc, LEDGER89_ERANGE);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.end, test_u64(4));

    /* from == end is a no-op with no revision change. */
    rc = ledger89_truncate_from(f.l, test_u64(4));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(st.revision.hi, 0u);
    CHECK_EQ(st.revision.lo, 0u);
    CHECK_U64(st.end, test_u64(4));
    fx_close(&f);
}

static void test_rewind_and_reuse(void)
{
    fx f;
    ledger89_state st;
    ledger89_slice s;
    unsigned char buf[4];
    size_t size;
    ledger89_index first;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    fill(&f, 4u);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_truncate_from(f.l, test_u64(2));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.end, test_u64(2));
    CHECK_U64(st.stable_end, test_u64(2));
    CHECK_EQ(st.revision.lo, 1u);
    rc = ledger89_read(f.l, test_u64(2), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_ENOENT);

    s.data = "z";
    s.size = 1u;
    rc = ledger89_appendv(f.l, &s, 1u, &first);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(first, test_u64(2));
    rc = ledger89_read(f.l, test_u64(2), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'z');
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    fx_close(&f);

    /* Truncation and replacement are durable. */
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.end, test_u64(3));
    CHECK_U64(st.stable_end, test_u64(3));
    CHECK_EQ(st.revision.lo, 1u);
    rc = ledger89_read(f.l, test_u64(2), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'z');
    fx_close(&f);
}

static void test_empty(void)
{
    fx f;
    ledger89_state st;
    ledger89_index first;
    ledger89_slice s;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    fill(&f, 3u);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_truncate_from(f.l, test_u64(1));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.first, test_u64(1));
    CHECK_U64(st.end, test_u64(1));
    CHECK_EQ(st.revision.lo, 1u);
    s.data = "q";
    s.size = 1u;
    rc = ledger89_appendv(f.l, &s, 1u, &first);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(first, test_u64(1));
    fx_close(&f);
}

int main(void)
{
    test_preconditions();
    test_rewind_and_reuse();
    test_empty();
    TEST_END;
}
