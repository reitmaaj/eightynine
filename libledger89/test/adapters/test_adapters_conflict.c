/* test_adapters_conflict.c - AB04, AB05: conflict replacement truncates the
 * old suffix and appends fresh records with no gap and no resurrection;
 * truncating below the base is rejected. */

#include <string.h>

#include "test.h"

#include "fixture.h"

#define GOT_MAX 8

static int collect(fx *f, ledger89_index *idx, unsigned long *tags, size_t cap,
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

int main(void)
{
    fx f;
    ledger89_record batch[5];
    ledger89_index idx[GOT_MAX];
    unsigned long tags[GOT_MAX];
    ledger89_view v;
    unsigned char old_data[2];
    unsigned char new_data[2];
    size_t count;
    size_t i;

    old_data[0] = 'o';
    old_data[1] = 'o';
    new_data[0] = 'n';
    new_data[1] = 'n';

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    for (i = 0u; i < 5u; ++i)
    {
        fx_record(&batch[i], (ledger89_index)(i + 1u),
                  (unsigned long)(i + 1u) * 10ul, old_data, sizeof old_data);
    }
    CHECK_EQ(ledger89_append(f.l, batch, 5u), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);

    /* AB04: replace the suffix after index 2. */
    CHECK_EQ(ledger89_truncate_after(f.l, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 2ul);
    fx_record(&batch[0], 3ul, 33ul, new_data, sizeof new_data);
    fx_record(&batch[1], 4ul, 44ul, new_data, sizeof new_data);
    CHECK_EQ(ledger89_append(f.l, batch, 2u), LEDGER89_OK);
    CHECK_EQ(collect(&f, idx, tags, GOT_MAX, &count), LEDGER89_OK);
    CHECK_EQ(count, 4u);
    CHECK_EQ(idx[2], 3ul);
    CHECK_EQ(tags[2], 33ul);
    CHECK_EQ(idx[3], 4ul);
    CHECK_EQ(tags[3], 44ul);

    /* The replacement survives a reopen; the old suffix is gone. */
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(fx_read(&f, 3ul, &v), LEDGER89_OK);
    CHECK_EQ(v.tag, 33ul);
    CHECK_EQ(v.size, 2u);
    CHECK_EQ(memcmp(v.data, new_data, 2u), 0);
    CHECK_EQ(fx_read(&f, 5ul, &v), LEDGER89_ERR_NOTFOUND);

    /* AB05: below base - 1 is rejected and changes nothing. */
    CHECK_EQ(ledger89_discard_before(f.l, 3ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 3ul);
    CHECK_EQ(ledger89_last_index(f.l), 4ul);
    CHECK_EQ(ledger89_truncate_after(f.l, 1ul), LEDGER89_ERR_RANGE);
    CHECK_EQ(ledger89_first_index(f.l), 3ul);
    CHECK_EQ(ledger89_last_index(f.l), 4ul);
    CHECK_EQ(collect(&f, idx, tags, GOT_MAX, &count), LEDGER89_OK);
    CHECK_EQ(count, 2u);
    CHECK_EQ(idx[0], 3ul);
    CHECK_EQ(idx[1], 4ul);

    fx_close(&f);

    TEST_END;
}
