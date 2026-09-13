/* test_append.c - A01..A11 and B01..B07: append semantics and batch
 * atomicity. */

#include <string.h>

#include "test.h"

#include "fixture.h"

static int check_unchanged(fx *f, ledger89_index last)
{
    ledger89_view v;
    ledger89_index at;
    int rc;

    at = ledger89_last_index(f->l);
    if (at != last)
    {
        return 0;
    }
    rc = fx_read(f, last + 1ul, &v);
    if (rc != LEDGER89_ERR_NOTFOUND)
    {
        return 0;
    }
    return 1;
}

int main(void)
{
    fx f;
    ledger89_record batch[3];
    ledger89_record r;
    ledger89_view v;
    unsigned char binary[4];
    unsigned char dummy;
    unsigned char data[3];
    size_t i;

    binary[0] = 0x00;
    binary[1] = 0xFF;
    binary[2] = 0x00;
    binary[3] = 0xFF;
    data[0] = 'x';
    data[1] = 'y';
    data[2] = 'z';

    CHECK_EQ(fx_open(&f), LEDGER89_OK);

    /* A01/A06/A07/A08: first record, empty payload, binary payload, tag 0. */
    CHECK_EQ(fx_append(&f, 1ul, 0ul, data, 0u), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 1ul);
    CHECK_EQ(fx_read(&f, 1ul, &v), LEDGER89_OK);
    CHECK_EQ(v.index, 1ul);
    CHECK_EQ(v.tag, 0ul);
    CHECK_EQ(v.size, 0u);

    CHECK_EQ(fx_append(&f, 2ul, 42ul, binary, sizeof binary), LEDGER89_OK);
    CHECK_EQ(fx_read(&f, 2ul, &v), LEDGER89_OK);
    CHECK_EQ(v.size, sizeof binary);
    CHECK(memcmp(v.data, binary, sizeof binary) == 0);

    /* A09: maximum tag. */
    CHECK_EQ(fx_append(&f, 3ul, (unsigned long)-1, data, sizeof data),
             LEDGER89_OK);
    CHECK_EQ(fx_read(&f, 3ul, &v), LEDGER89_OK);
    CHECK_EQ(v.tag, (unsigned long)-1);

    /* A03/A04/A05: sequence errors leave the ledger unchanged. */
    CHECK_EQ(fx_append(&f, 3ul, 1ul, data, sizeof data), LEDGER89_ERR_SEQUENCE);
    CHECK_EQ(fx_append(&f, 5ul, 1ul, data, sizeof data), LEDGER89_ERR_SEQUENCE);
    CHECK_EQ(fx_append(&f, 2ul, 1ul, data, sizeof data), LEDGER89_ERR_SEQUENCE);
    CHECK(check_unchanged(&f, 3ul) != 0);

    /* A11: oversized payload. */
    fx_record(&r, 4ul, 1ul, &dummy, (size_t)LEDGER89_MAX_RECORD_BYTES + 1u);
    CHECK_EQ(ledger89_append(f.l, &r, 1u), LEDGER89_ERR_ARG);
    CHECK(check_unchanged(&f, 3ul) != 0);

    /* B01: batch of one. */
    fx_record(&batch[0], 4ul, 4ul, data, sizeof data);
    CHECK_EQ(ledger89_append(f.l, batch, 1u), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 4ul);

    /* B03: zero count. */
    CHECK_EQ(ledger89_append(f.l, batch, 0u), LEDGER89_ERR_ARG);
    CHECK_EQ(ledger89_append(f.l, NULL, 0u), LEDGER89_ERR_ARG);
    CHECK(check_unchanged(&f, 4ul) != 0);

    /* B04: bad first index. */
    fx_record(&batch[0], 5ul, 1ul, data, sizeof data);
    fx_record(&batch[1], 6ul, 1ul, data, sizeof data);
    CHECK_EQ(ledger89_append(f.l, batch, 2u), LEDGER89_OK);

    fx_record(&batch[0], 7ul, 1ul, data, sizeof data);
    fx_record(&batch[1], 8ul, 1ul, data, sizeof data);
    CHECK_EQ(ledger89_append(f.l, batch, 2u), LEDGER89_OK);

    fx_record(&batch[0], 10ul, 1ul, data, sizeof data);
    fx_record(&batch[1], 11ul, 1ul, data, sizeof data);
    CHECK_EQ(ledger89_append(f.l, batch, 2u), LEDGER89_ERR_SEQUENCE);
    CHECK(check_unchanged(&f, 8ul) != 0);

    /* B05: gap inside a batch. */
    fx_record(&batch[0], 9ul, 1ul, data, sizeof data);
    fx_record(&batch[1], 11ul, 1ul, data, sizeof data);
    CHECK_EQ(ledger89_append(f.l, batch, 2u), LEDGER89_ERR_SEQUENCE);
    CHECK(check_unchanged(&f, 8ul) != 0);

    /* B07: invalid record midway rejects the whole batch. */
    fx_record(&batch[0], 9ul, 1ul, data, sizeof data);
    fx_record(&batch[1], 10ul, 1ul, &dummy,
              (size_t)LEDGER89_MAX_RECORD_BYTES + 1u);
    CHECK_EQ(ledger89_append(f.l, batch, 2u), LEDGER89_ERR_ARG);
    CHECK(check_unchanged(&f, 8ul) != 0);

    /* B02: one batch of 100 contiguous records. */
    {
        ledger89_record many[100];

        for (i = 0u; i < 100u; ++i)
        {
            fx_record(&many[i], 9ul + (ledger89_index)i, (unsigned long)i, data,
                      sizeof data);
        }
        CHECK_EQ(ledger89_append(f.l, many, 100u), LEDGER89_OK);
    }
    CHECK_EQ(ledger89_last_index(f.l), 108ul);

    /* NULL data with positive size. */
    CHECK_EQ(fx_append(&f, 109ul, 1ul, NULL, 1u), LEDGER89_ERR_ARG);
    CHECK(check_unchanged(&f, 108ul) != 0);

    /* Reopen preserves everything. */
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 108ul);
    CHECK_EQ(fx_read(&f, 1ul, &v), LEDGER89_OK);
    CHECK_EQ(v.size, 0u);
    CHECK_EQ(fx_read(&f, 2ul, &v), LEDGER89_OK);
    CHECK(memcmp(v.data, binary, sizeof binary) == 0);
    CHECK_EQ(fx_read(&f, 108ul, &v), LEDGER89_OK);

    fx_close(&f);

    TEST_END;
}
