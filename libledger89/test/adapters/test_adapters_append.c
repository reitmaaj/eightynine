/* test_adapters_append.c - AB01..AB03: a replication adapter appends
 * contiguous batches, syncs, reopens, and replays by iteration; duplicate
 * and gap appends are rejected without side effects. */

#include <string.h>

#include "test.h"

#include "fixture.h"

#define REPLAY_MAX 8

static int replay(fx *f, ledger89_index *idx, unsigned long *tags, size_t cap,
                  size_t *count)
{
    ledger89_iter *it;
    ledger89_view v;
    size_t n;
    int rc;

    it = NULL;
    *count = 0u;
    rc = ledger89_iter_open(f->l, 0ul, 0ul, &it);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    n = 0u;
    for (;;)
    {
        rc = ledger89_iter_next(it, &v);
        if (rc == LEDGER89_END)
        {
            break;
        }
        if (rc != LEDGER89_OK)
        {
            ledger89_iter_close(it);
            return rc;
        }
        if (n >= cap)
        {
            ledger89_iter_close(it);
            return LEDGER89_ERR_RANGE;
        }
        idx[n] = v.index;
        tags[n] = v.tag;
        ++n;
    }
    ledger89_iter_close(it);
    *count = n;
    return LEDGER89_OK;
}

static void fill_batch(ledger89_record *batch, ledger89_index first,
                       size_t count, unsigned long tag_base,
                       const unsigned char *data, size_t size)
{
    size_t i;

    for (i = 0u; i < count; ++i)
    {
        fx_record(&batch[i], first + (ledger89_index)i,
                  tag_base + (unsigned long)i, data, size);
    }
}

int main(void)
{
    fx f;
    ledger89_record batch[3];
    ledger89_index idx[REPLAY_MAX];
    unsigned long tags[REPLAY_MAX];
    unsigned char data[2];
    size_t count;
    size_t i;

    data[0] = 'x';
    data[1] = 'y';

    CHECK_EQ(fx_open(&f), LEDGER89_OK);

    /* AB01: two contiguous batches, sync, reopen, replay. */
    fill_batch(batch, 1ul, 3u, 100ul, data, sizeof data);
    CHECK_EQ(ledger89_append(f.l, batch, 3u), LEDGER89_OK);
    fill_batch(batch, 4ul, 3u, 400ul, data, sizeof data);
    CHECK_EQ(ledger89_append(f.l, batch, 3u), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(replay(&f, idx, tags, REPLAY_MAX, &count), LEDGER89_OK);
    CHECK_EQ(count, 6u);
    for (i = 0u; i < 6u; ++i)
    {
        CHECK_EQ(idx[i], (ledger89_index)(i + 1u));
    }
    CHECK_EQ(tags[0], 100ul);
    CHECK_EQ(tags[2], 102ul);
    CHECK_EQ(tags[3], 400ul);
    CHECK_EQ(tags[5], 402ul);

    /* AB02: duplicate index rejected, state unchanged. */
    CHECK_EQ(fx_append(&f, 6ul, 999ul, data, sizeof data),
             LEDGER89_ERR_SEQUENCE);
    CHECK_EQ(fx_append(&f, 3ul, 999ul, data, sizeof data),
             LEDGER89_ERR_SEQUENCE);
    CHECK_EQ(ledger89_last_index(f.l), 6ul);

    /* AB03: gap index rejected, state unchanged. */
    CHECK_EQ(fx_append(&f, 8ul, 999ul, data, sizeof data),
             LEDGER89_ERR_SEQUENCE);
    CHECK_EQ(ledger89_last_index(f.l), 6ul);
    CHECK_EQ(replay(&f, idx, tags, REPLAY_MAX, &count), LEDGER89_OK);
    CHECK_EQ(count, 6u);
    CHECK_EQ(tags[5], 402ul);

    fx_close(&f);

    TEST_END;
}
