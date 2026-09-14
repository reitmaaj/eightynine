/* test_adapters_append.c - WAL-style consumer: envelope records, replay, and
 * conflict detection through compare-and-append. */

#include "fixture.h"
#include "test.h"

typedef struct checkpoint
{
    ledger89_id id;
    ledger89_revision revision;
    ledger89_index next;
} checkpoint;

static int append_op(ledger89 *l, unsigned char op, unsigned char value,
                     ledger89_index *out)
{
    unsigned char buf[2];
    ledger89_slice s;

    buf[0] = op;
    buf[1] = value;
    s.data = buf;
    s.size = 2u;
    return ledger89_appendv(l, &s, 1u, out);
}

static int replay(ledger89 *l, ledger89_index from, unsigned long *count)
{
    ledger89_iter it;
    ledger89_index idx;
    unsigned char buf[2];
    size_t size;
    int rc;

    *count = 0ul;
    rc = ledger89_iter_init(&it, l, from);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    for (;;)
    {
        rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
        if (rc != LEDGER89_OK)
        {
            break;
        }
        if (size != 2u || buf[0] != 1u)
        {
            return LEDGER89_ECORRUPT;
        }
        ++(*count);
    }
    return rc;
}

int main(void)
{
    fx f;
    ledger89_index first;
    ledger89_index idx;
    ledger89_state st;
    checkpoint cp;
    unsigned long count;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    CHECK_EQ(append_op(f.l, 1u, 0xAAu, &first), LEDGER89_OK);
    CHECK_U64(first, test_u64(1));
    CHECK_EQ(append_op(f.l, 1u, 0xBBu, NULL), LEDGER89_OK);
    CHECK_EQ(append_op(f.l, 1u, 0xCCu, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(replay(f.l, test_u64(1), &count), LEDGER89_DONE);
    CHECK_EQ(count, 3ul);

    /* The consumer records a checkpoint. */
    CHECK_EQ(ledger89_get_state(f.l, &st), LEDGER89_OK);
    cp.id = st.id;
    cp.revision = st.revision;
    cp.next = st.stable_end;

    /* A duplicate append attempt against a stale end is refused. */
    rc = ledger89_appendv_at(f.l, cp.revision, test_u64(1), NULL, 0u, NULL);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    {
        unsigned char eb[2];
        ledger89_slice s;

        eb[0] = 1u;
        eb[1] = 0xDDu;
        s.data = eb;
        s.size = 2u;
        rc = ledger89_appendv_at(f.l, cp.revision, test_u64(2), &s, 1u, NULL);
        CHECK_EQ(rc, LEDGER89_ESTALE);
        rc = ledger89_appendv_at(f.l, cp.revision, cp.next, &s, 1u, &idx);
        CHECK_EQ(rc, LEDGER89_OK);
        CHECK_U64(idx, test_u64(4));
    }
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    fx_close(&f);

    /* Replay from the checkpoint after reopen. */
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(f.l, &st), LEDGER89_OK);
    CHECK(memcmp(st.id.bytes, cp.id.bytes, 16u) == 0);
    CHECK(ledger89_u64_equal(st.revision, cp.revision) != 0);
    CHECK_EQ(replay(f.l, cp.next, &count), LEDGER89_DONE);
    CHECK_EQ(count, 1ul);
    fx_close(&f);
    TEST_END;
}
