/* test_stress_many_segments.c - ST01, ST03: one record per sealed segment
 * across many segments; iteration, cross-segment reads, and recovery stay
 * exact. `just long` rebuilds this with MANY_RECORDS=2000. */

#include <string.h>

#include "test.h"

#include "fixture.h"

#ifndef MANY_RECORDS
#define MANY_RECORDS 200ul
#endif

static int check_range(fx *f, ledger89_index first, ledger89_index last)
{
    ledger89_iter *it;
    ledger89_view v;
    ledger89_index want;
    int rc;

    it = NULL;
    rc = ledger89_iter_open(f->l, first, last, &it);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    want = first;
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
        if (v.index != want)
        {
            ledger89_iter_close(it);
            return LEDGER89_ERR_CORRUPT;
        }
        if (v.tag != (unsigned long)want)
        {
            ledger89_iter_close(it);
            return LEDGER89_ERR_CORRUPT;
        }
        if (v.size != 64u)
        {
            ledger89_iter_close(it);
            return LEDGER89_ERR_CORRUPT;
        }
        ++want;
    }
    ledger89_iter_close(it);
    return want == last + 1ul ? LEDGER89_OK : LEDGER89_ERR_CORRUPT;
}

int main(void)
{
    fx f;
    ledger89_index i;
    ledger89_view v;
    unsigned char data[64];

    memset(data, 0x5A, sizeof data);
    CHECK_EQ(fx_open_limits(&f, 1048576ul, 1ul), LEDGER89_OK);
    for (i = 1ul; i <= MANY_RECORDS; ++i)
    {
        data[0] = (unsigned char)(i & 0xFFul);
        CHECK_EQ(fx_append(&f, i, (unsigned long)i, data, sizeof data),
                 LEDGER89_OK);
    }
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), MANY_RECORDS);
    CHECK_EQ(fx_count_sealed(&f), (int)(MANY_RECORDS - 1ul));
    CHECK_EQ(check_range(&f, 1ul, MANY_RECORDS), LEDGER89_OK);

    /* Reads that stride across segment boundaries. */
    for (i = 1ul; i <= MANY_RECORDS; i += 97ul)
    {
        CHECK_EQ(fx_read(&f, i, &v), LEDGER89_OK);
        CHECK_EQ(v.tag, (unsigned long)i);
        CHECK_EQ(v.size, sizeof data);
        CHECK_EQ(((const unsigned char *)v.data)[0],
                 (unsigned char)(i & 0xFFul));
    }

    /* ST03: recovery restores the exact sequence. */
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 1ul);
    CHECK_EQ(ledger89_last_index(f.l), MANY_RECORDS);
    CHECK_EQ(fx_count_sealed(&f), (int)(MANY_RECORDS - 1ul));
    CHECK_EQ(check_range(&f, 1ul, MANY_RECORDS), LEDGER89_OK);
    fx_close(&f);

    TEST_END;
}
