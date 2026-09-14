/* test_adapters_projection.c - query-engine style checkpointing over ledger
 * identity, revision, and positions. */

#include "fixture.h"
#include "test.h"

typedef struct checkpoint
{
    ledger89_id id;
    ledger89_revision revision;
    ledger89_index next;
} checkpoint;

/* Scan [from, stable_end) and return the next position to index. */
static int index_through(ledger89 *l, ledger89_index from,
                         ledger89_index *next_out)
{
    ledger89_iter it;
    ledger89_index idx;
    size_t size;
    int rc;

    *next_out = from;
    rc = ledger89_iter_init(&it, l, from);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    for (;;)
    {
        rc = ledger89_iter_next(&it, &idx, NULL, 0u, &size);
        if (rc != LEDGER89_OK)
        {
            break;
        }
        *next_out = test_u64((unsigned long)idx.lo + 1ul);
    }
    return rc;
}

static int rebuild_count(ledger89 *l, ledger89_index from, unsigned long *count)
{
    ledger89_iter it;
    ledger89_index idx;
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
        rc = ledger89_iter_next(&it, &idx, NULL, 0u, &size);
        if (rc != LEDGER89_OK)
        {
            break;
        }
        ++(*count);
    }
    return rc;
}

int main(void)
{
    fx f;
    ledger89_state st;
    checkpoint cp;
    ledger89_index next;
    ledger89_slice s;
    unsigned char b;
    unsigned long count;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    b = 'a';
    s.data = &b;
    s.size = 1u;
    CHECK_EQ(ledger89_appendv(f.l, &s, 1u, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_appendv(f.l, &s, 1u, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(f.l, &st), LEDGER89_OK);
    cp.id = st.id;
    cp.revision = st.revision;
    cp.next = st.stable_end;

    /* Incremental projection: index the new stable records. */
    b = 'b';
    CHECK_EQ(ledger89_appendv(f.l, &s, 1u, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    rc = index_through(f.l, cp.next, &next);
    CHECK_EQ(rc, LEDGER89_DONE);
    CHECK_U64(next, test_u64(4));
    cp.next = next;

    /* Suffix truncation changes the revision: the consumer must rebuild. */
    CHECK_EQ(ledger89_truncate_from(f.l, test_u64(2)), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(f.l, &st), LEDGER89_OK);
    CHECK(ledger89_u64_equal(st.revision, cp.revision) == 0);
    CHECK_EQ(rebuild_count(f.l, st.first, &count), LEDGER89_DONE);
    CHECK_EQ(count, 1ul);

    /* Pruning before the checkpoint yields EGONE. */
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(f.l), LEDGER89_OK);
    b = 'c';
    CHECK_EQ(ledger89_appendv(f.l, &s, 1u, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    {
        ledger89_index actual;

        CHECK_EQ(ledger89_prune_before(f.l, test_u64(2), &actual), LEDGER89_OK);
        CHECK_U64(actual, test_u64(2));
    }
    {
        ledger89_iter it;

        rc = ledger89_iter_init(&it, f.l, test_u64(1));
        CHECK_EQ(rc, LEDGER89_EGONE);
    }
    fx_close(&f);
    TEST_END;
}
