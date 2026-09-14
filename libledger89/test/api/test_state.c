/* test_state.c - coherent state snapshots and identity/revision rules. */

#include "fixture.h"
#include "test.h"

int main(void)
{
    fx f;
    ledger89_state st;
    ledger89_state st2;
    ledger89_id id;
    ledger89_slice s;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    rc = ledger89_get_state(NULL, &st);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    rc = ledger89_get_state(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    id = st.id;
    CHECK_U64(st.first, test_u64(1));
    CHECK_U64(st.stable_end, test_u64(1));
    CHECK_U64(st.end, test_u64(1));

    s.data = "x";
    s.size = 1u;
    rc = ledger89_appendv(f.l, &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.end, test_u64(2));
    CHECK_U64(st.stable_end, test_u64(1));
    CHECK(memcmp(st.id.bytes, id.bytes, 16u) == 0);
    CHECK_EQ(st.revision.lo, 0u);

    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_truncate_from(f.l, test_u64(1));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st2);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(st2.revision.lo, 1u);
    CHECK(memcmp(st2.id.bytes, id.bytes, 16u) == 0);
    CHECK_U64(st2.first, test_u64(1));

    fx_close(&f);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK(memcmp(st.id.bytes, id.bytes, 16u) == 0);
    CHECK_EQ(st.revision.lo, 1u);
    fx_close(&f);
    TEST_END;
}
