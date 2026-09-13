/* test_sync.c - D01..D07: logical visibility, sync, and clean reopen. */

#include <string.h>

#include "test.h"

#include "fixture.h"

static int iter_count(fx *f, size_t *count)
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
        ++n;
    }
    ledger89_iter_close(it);
    *count = n;
    return LEDGER89_OK;
}

int main(void)
{
    fx f;
    ledger89_record batch[100];
    ledger89_view v;
    size_t count;
    unsigned char payload[1];
    ledger89_index i;

    payload[0] = 'z';
    CHECK_EQ(fx_open(&f), LEDGER89_OK);

    /* D07: sync on an empty ledger. */
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(NULL), LEDGER89_ERR_ARG);

    /* D01/D02: visibility before sync. */
    CHECK_EQ(fx_append(&f, 1ul, 1ul, payload, sizeof payload), LEDGER89_OK);
    CHECK_EQ(fx_read(&f, 1ul, &v), LEDGER89_OK);
    CHECK_EQ(v.index, 1ul);
    CHECK_EQ(iter_count(&f, &count), LEDGER89_OK);
    CHECK_EQ(count, 1u);

    /* D03: sync, close, reopen. */
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(fx_read(&f, 1ul, &v), LEDGER89_OK);
    CHECK_EQ(((const unsigned char *)v.data)[0], (unsigned char)'z');

    /* D03b: continue after reopen. */
    CHECK_EQ(fx_append(&f, 2ul, 2ul, payload, sizeof payload), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);

    /* D05: one batch, one sync. */
    for (i = 0ul; i < 3ul; ++i)
    {
        fx_record(&batch[i], 3ul + i, 3ul + i, payload, sizeof payload);
    }
    CHECK_EQ(ledger89_append(f.l, batch, 3u), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 5ul);

    /* D04: 100 appends, one sync. */
    for (i = 0ul; i < 100ul; ++i)
    {
        fx_record(&batch[0], 6ul + i, (unsigned long)i, payload,
                  sizeof payload);
        CHECK_EQ(ledger89_append(f.l, batch, 1u), LEDGER89_OK);
    }
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 105ul);
    CHECK_EQ(fx_read(&f, 105ul, &v), LEDGER89_OK);
    CHECK_EQ(v.tag, 99ul);

    /* D06: repeated sync on a clean ledger. */
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);

    fx_close(&f);

    TEST_END;
}
