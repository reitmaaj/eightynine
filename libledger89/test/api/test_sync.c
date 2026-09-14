/* test_sync.c - durability frontier semantics. */

#include "fixture.h"
#include "test.h"

static void test_frontier(void)
{
    fx f;
    ledger89_slice s;
    ledger89_index stable;
    ledger89_state st;
    unsigned char buf[4];
    size_t size;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    s.data = "abc";
    s.size = 3u;
    rc = ledger89_appendv(f.l, &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.end, test_u64(2));
    CHECK_U64(st.stable_end, test_u64(1));

    /* Unstable records are readable. */
    rc = ledger89_read(f.l, test_u64(1), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(size, 3u);

    stable = ledger89_u64_zero();
    rc = ledger89_sync(f.l, &stable);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(stable, test_u64(2));
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.stable_end, test_u64(2));

    /* Sync on a clean ledger is a no-op. */
    rc = ledger89_sync(f.l, &stable);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(stable, test_u64(2));
    fx_close(&f);
}

static void test_unsynced_tail_dropped(void)
{
    fx f;
    ledger89_slice s;
    ledger89_state st;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    s.data = "a";
    s.size = 1u;
    rc = ledger89_appendv(f.l, &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    s.data = "b";
    rc = ledger89_appendv(f.l, &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.end, test_u64(3));
    CHECK_U64(st.stable_end, test_u64(2));
    fx_close(&f);

    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.end, test_u64(2));
    CHECK_U64(st.stable_end, test_u64(2));
    fx_close(&f);
}

int main(void)
{
    test_frontier();
    test_unsynced_tail_dropped();
    TEST_END;
}
