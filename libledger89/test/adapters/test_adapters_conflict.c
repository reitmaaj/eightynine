/* test_adapters_conflict.c - replicated-log style conflict replacement:
 * truncate a suffix, append replacements, never resurrect old records. */

#include "fixture.h"
#include "test.h"

static int append_byte(ledger89 *l, unsigned char b, ledger89_index *out)
{
    ledger89_slice s;

    s.data = &b;
    s.size = 1u;
    return ledger89_appendv(l, &s, 1u, out);
}

int main(void)
{
    fx f;
    ledger89_state st;
    ledger89_revision stale_revision;
    ledger89_index stale_end;
    ledger89_index idx;
    unsigned char buf[2];
    size_t size;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    CHECK_EQ(append_byte(f.l, 'a', NULL), LEDGER89_OK);
    CHECK_EQ(append_byte(f.l, 'b', NULL), LEDGER89_OK);
    CHECK_EQ(append_byte(f.l, 'c', NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(f.l, &st), LEDGER89_OK);
    stale_revision = st.revision;
    stale_end = st.end;

    /* The follower's suffix conflicts and is replaced. */
    rc = ledger89_truncate_from(f.l, test_u64(2));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(st.revision.lo, 1u);
    CHECK_U64(st.end, test_u64(2));

    /* The old assumption is now stale. */
    rc = ledger89_appendv_at(f.l, stale_revision, stale_end, NULL, 0u, NULL);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    {
        unsigned char x;
        ledger89_slice s;

        x = 'x';
        s.data = &x;
        s.size = 1u;
        rc = ledger89_appendv_at(f.l, stale_revision, stale_end, &s, 1u, NULL);
        CHECK_EQ(rc, LEDGER89_ESTALE);
        rc = ledger89_appendv_at(f.l, st.revision, st.end, &s, 1u, &idx);
        CHECK_EQ(rc, LEDGER89_OK);
        CHECK_U64(idx, test_u64(2));
        x = 'y';
        rc = ledger89_appendv_at(f.l, st.revision, test_u64(3), &s, 1u, &idx);
        CHECK_EQ(rc, LEDGER89_OK);
        CHECK_U64(idx, test_u64(3));
    }
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);

    /* Old record 'b' must not be visible at index 2. */
    rc = ledger89_read(f.l, test_u64(2), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(size, 1u);
    CHECK_EQ(buf[0], (unsigned char)'x');
    rc = ledger89_read(f.l, test_u64(3), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'y');
    fx_close(&f);
    TEST_END;
}
