/* test_append.c - append batch assignment, validation, and compare-and-append.
 */

#include "fixture.h"
#include "test.h"

static void test_assignment(void)
{
    fx f;
    ledger89_slice slices[3];
    ledger89_index first;
    ledger89_state st;
    unsigned char buf[8];
    size_t size;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    slices[0].data = "a";
    slices[0].size = 1u;
    slices[1].data = "bb";
    slices[1].size = 2u;
    slices[2].data = NULL;
    slices[2].size = 0u;
    first = ledger89_u64_zero();
    rc = ledger89_appendv(f.l, slices, 3u, &first);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(first, test_u64(1));
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.end, test_u64(4));
    CHECK_U64(st.stable_end, test_u64(1));

    rc = ledger89_append(f.l, "ccc", 3u, &first);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(first, test_u64(4));
    rc = ledger89_read(f.l, test_u64(4), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(size, 3u);
    CHECK(memcmp(buf, "ccc", 3u) == 0);
    fx_close(&f);
}

static void test_rejections(void)
{
    fx f;
    ledger89_slice s;
    ledger89_index first;
    ledger89_state before;
    ledger89_state after;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    s.data = "x";
    s.size = 1u;
    rc = ledger89_appendv(f.l, NULL, 0u, NULL);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    rc = ledger89_appendv(f.l, NULL, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    s.data = NULL;
    s.size = 1u;
    rc = ledger89_appendv(f.l, &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    s.size = (size_t)LEDGER89_MAX_RECORD_BYTES + 1u;
    rc = ledger89_appendv(f.l, &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_ERANGE);
    rc = ledger89_get_state(f.l, &before);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(f.l, &after);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(before.end, after.end);
    first = test_u64(99);
    rc = ledger89_appendv(f.l, NULL, 1u, &first);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    CHECK_U64(first, test_u64(99));
    fx_close(&f);
}

static void test_compare_and_append(void)
{
    fx f;
    ledger89_slice s;
    ledger89_index first;
    ledger89_state st;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    s.data = "x";
    s.size = 1u;
    rc = ledger89_appendv(f.l, &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);

    rc = ledger89_appendv_at(f.l, st.revision, test_u64(9), &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_ESTALE);
    rc = ledger89_appendv_at(f.l, st.revision, st.end, &s, 1u, &first);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(first, test_u64(2));
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);

    /* ABA: truncate and replace, then retry the original assumption. */
    rc = ledger89_truncate_from(f.l, test_u64(2));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_appendv_at(f.l, st.revision, test_u64(2), &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_ESTALE);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_appendv_at(f.l, st.revision, st.end, &s, 1u, &first);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(first, test_u64(2));
    fx_close(&f);
}

int main(void)
{
    test_assignment();
    test_rejections();
    test_compare_and_append();
    TEST_END;
}
