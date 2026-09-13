/* test_truncate.c - T01..T15 and the canonical branch replacement. */

#include <string.h>

#include "test.h"

#include "fixture.h"

static int append_byte(fx *f, ledger89_index index, unsigned long tag,
                       unsigned char value)
{
    return fx_append(f, index, tag, &value, 1u);
}

static int read_byte(fx *f, ledger89_index index, unsigned long tag,
                     unsigned char value)
{
    ledger89_view v;

    if (fx_read(f, index, &v) != LEDGER89_OK)
    {
        return 0;
    }
    if (v.tag != tag)
    {
        return 0;
    }
    if (v.size != 1u)
    {
        return 0;
    }
    if (((const unsigned char *)v.data)[0] != value)
    {
        return 0;
    }
    return 1;
}

static int fill(fx *f, ledger89_index count)
{
    ledger89_index i;

    for (i = 1ul; i <= count; ++i)
    {
        if (append_byte(f, i, (unsigned long)i, (unsigned char)('A' + i)) !=
            LEDGER89_OK)
        {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    fx f;
    ledger89_view v;

    /* T04/T09/T10/T15: truncate inside the active segment, then replace. */
    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    CHECK(fill(&f, 4ul) != 0);
    CHECK_EQ(ledger89_truncate_after(f.l, 4ul), LEDGER89_OK);
    CHECK_EQ(ledger89_truncate_after(f.l, 99ul), LEDGER89_OK);
    CHECK_EQ(ledger89_truncate_after(f.l, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 2ul);
    CHECK_EQ(fx_read(&f, 3ul, &v), LEDGER89_ERR_NOTFOUND);
    CHECK(read_byte(&f, 2ul, 2ul, (unsigned char)('A' + 2)) != 0);
    CHECK_EQ(ledger89_truncate_after(f.l, 2ul), LEDGER89_OK);
    CHECK_EQ(append_byte(&f, 3ul, 33ul, (unsigned char)'X'), LEDGER89_OK);
    CHECK_EQ(append_byte(&f, 4ul, 44ul, (unsigned char)'Y'), LEDGER89_OK);
    CHECK(read_byte(&f, 3ul, 33ul, (unsigned char)'X') != 0);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 4ul);
    CHECK(read_byte(&f, 1ul, 1ul, (unsigned char)('A' + 1)) != 0);
    CHECK(read_byte(&f, 3ul, 33ul, (unsigned char)'X') != 0);
    CHECK(read_byte(&f, 4ul, 44ul, (unsigned char)'Y') != 0);

    /* T03: empty the ledger while preserving the base. */
    CHECK_EQ(ledger89_truncate_after(f.l, 0ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 1ul);
    CHECK_EQ(ledger89_last_index(f.l), 0ul);
    CHECK_EQ(fx_read(&f, 1ul, &v), LEDGER89_ERR_NOTFOUND);
    CHECK_EQ(append_byte(&f, 1ul, 9ul, (unsigned char)'Z'), LEDGER89_OK);
    CHECK(read_byte(&f, 1ul, 9ul, (unsigned char)'Z') != 0);
    fx_close(&f);

    /* T06: truncate inside a sealed segment. */
    CHECK_EQ(fx_open_limits(&f, 0ul, 4ul), LEDGER89_OK);
    CHECK(fill(&f, 8ul) != 0);
    CHECK_EQ(fx_count_sealed(&f), 1);
    CHECK_EQ(ledger89_truncate_after(f.l, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 2ul);
    CHECK_EQ(fx_count_sealed(&f), 1);
    CHECK_EQ(fx_read(&f, 3ul, &v), LEDGER89_ERR_NOTFOUND);
    CHECK(read_byte(&f, 2ul, 2ul, (unsigned char)('A' + 2)) != 0);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 2ul);
    CHECK(read_byte(&f, 1ul, 1ul, (unsigned char)('A' + 1)) != 0);
    CHECK_EQ(fx_read(&f, 3ul, &v), LEDGER89_ERR_NOTFOUND);
    fx_close(&f);

    /* T05/T07: truncate exactly at a sealed segment end. */
    CHECK_EQ(fx_open_limits(&f, 0ul, 4ul), LEDGER89_OK);
    CHECK(fill(&f, 8ul) != 0);
    CHECK_EQ(ledger89_truncate_after(f.l, 4ul), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 4ul);
    CHECK(read_byte(&f, 4ul, 4ul, (unsigned char)('A' + 4)) != 0);
    CHECK_EQ(fx_read(&f, 5ul, &v), LEDGER89_ERR_NOTFOUND);
    fx_close(&f);

    /* T08: remove several whole sealed segments. */
    CHECK_EQ(fx_open_limits(&f, 0ul, 2ul), LEDGER89_OK);
    CHECK(fill(&f, 8ul) != 0);
    CHECK_EQ(fx_count_sealed(&f), 3);
    CHECK_EQ(ledger89_truncate_after(f.l, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 2ul);
    CHECK_EQ(fx_count_sealed(&f), 1);
    CHECK_EQ(fx_read(&f, 3ul, &v), LEDGER89_ERR_NOTFOUND);
    fx_close(&f);

    /* T02/T13: truncate after the first record and below the base. */
    CHECK_EQ(fx_open_limits(&f, 0ul, 4ul), LEDGER89_OK);
    CHECK(fill(&f, 8ul) != 0);
    CHECK_EQ(ledger89_truncate_after(f.l, 1ul), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 1ul);
    CHECK(read_byte(&f, 1ul, 1ul, (unsigned char)('A' + 1)) != 0);
    fx_close(&f);

    /* Iterator staleness across a structural mutation. */
    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    CHECK(fill(&f, 4ul) != 0);
    {
        ledger89_iter *it;

        it = NULL;
        CHECK_EQ(ledger89_iter_open(f.l, 0ul, 0ul, &it), LEDGER89_OK);
        CHECK_EQ(ledger89_truncate_after(f.l, 2ul), LEDGER89_OK);
        CHECK_EQ(ledger89_iter_next(it, &v), LEDGER89_ERR_STATE);
        ledger89_iter_close(it);
    }
    fx_close(&f);

    /* Canonical branch replacement across a sealed boundary. */
    CHECK_EQ(fx_open_limits(&f, 0ul, 4ul), LEDGER89_OK);
    CHECK(fill(&f, 4ul) != 0);
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(f.l), LEDGER89_OK);
    CHECK_EQ(ledger89_truncate_after(f.l, 2ul), LEDGER89_OK);
    CHECK_EQ(append_byte(&f, 3ul, 33ul, (unsigned char)'X'), LEDGER89_OK);
    CHECK_EQ(append_byte(&f, 4ul, 44ul, (unsigned char)'Y'), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 4ul);
    CHECK(read_byte(&f, 1ul, 1ul, (unsigned char)('A' + 1)) != 0);
    CHECK(read_byte(&f, 2ul, 2ul, (unsigned char)('A' + 2)) != 0);
    CHECK(read_byte(&f, 3ul, 33ul, (unsigned char)'X') != 0);
    CHECK(read_byte(&f, 4ul, 44ul, (unsigned char)'Y') != 0);
    fx_close(&f);

    TEST_END;
}
