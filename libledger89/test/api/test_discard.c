/* test_discard.c - D01..D07: prefix discard and base advancement. */

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

    /* D01: no-op at or before the first index. */
    CHECK_EQ(fx_open_limits(&f, 0ul, 2ul), LEDGER89_OK);
    CHECK(fill(&f, 8ul) != 0);
    CHECK_EQ(ledger89_discard_before(f.l, 0ul), LEDGER89_OK);
    CHECK_EQ(ledger89_discard_before(f.l, 1ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 1ul);
    CHECK_EQ(ledger89_last_index(f.l), 8ul);

    /* D04: discard across whole sealed segments. */
    CHECK_EQ(fx_count_sealed(&f), 3);
    CHECK_EQ(ledger89_discard_before(f.l, 3ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 3ul);
    CHECK_EQ(ledger89_last_index(f.l), 8ul);
    CHECK_EQ(fx_count_sealed(&f), 2);
    CHECK_EQ(fx_read(&f, 2ul, &v), LEDGER89_ERR_NOTFOUND);
    CHECK(read_byte(&f, 3ul, 3ul, (unsigned char)('A' + 3)) != 0);

    /* D03: discard inside a sealed segment. */
    CHECK_EQ(ledger89_discard_before(f.l, 4ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 4ul);
    CHECK_EQ(fx_read(&f, 3ul, &v), LEDGER89_ERR_NOTFOUND);
    CHECK(read_byte(&f, 4ul, 4ul, (unsigned char)('A' + 4)) != 0);
    CHECK_EQ(fx_count_sealed(&f), 2);

    /* D02: discard inside the active segment after removing all sealed. */
    CHECK_EQ(ledger89_discard_before(f.l, 7ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 7ul);
    CHECK_EQ(fx_count_sealed(&f), 0);
    CHECK_EQ(fx_read(&f, 6ul, &v), LEDGER89_ERR_NOTFOUND);
    CHECK(read_byte(&f, 7ul, 7ul, (unsigned char)('A' + 7)) != 0);
    CHECK(read_byte(&f, 8ul, 8ul, (unsigned char)('A' + 8)) != 0);

    /* D05/D06/D07: discard all, preserve the base, append at the base. */
    CHECK_EQ(ledger89_discard_before(f.l, 99ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 9ul);
    CHECK_EQ(ledger89_last_index(f.l), 8ul);
    CHECK_EQ(fx_read(&f, 8ul, &v), LEDGER89_ERR_NOTFOUND);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 9ul);
    CHECK_EQ(ledger89_last_index(f.l), 8ul);
    CHECK_EQ(append_byte(&f, 9ul, 9ul, (unsigned char)'Z'), LEDGER89_OK);
    CHECK(read_byte(&f, 9ul, 9ul, (unsigned char)'Z') != 0);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 9ul);
    CHECK_EQ(ledger89_last_index(f.l), 9ul);
    fx_close(&f);

    /* Discard into the active segment with sealed segments before it. */
    CHECK_EQ(fx_open_limits(&f, 0ul, 4ul), LEDGER89_OK);
    CHECK(fill(&f, 8ul) != 0);
    CHECK_EQ(fx_count_sealed(&f), 1);
    CHECK_EQ(ledger89_discard_before(f.l, 6ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 6ul);
    CHECK_EQ(fx_count_sealed(&f), 0);
    CHECK_EQ(fx_read(&f, 5ul, &v), LEDGER89_ERR_NOTFOUND);
    CHECK(read_byte(&f, 6ul, 6ul, (unsigned char)('A' + 6)) != 0);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 6ul);
    CHECK(read_byte(&f, 8ul, 8ul, (unsigned char)('A' + 8)) != 0);
    fx_close(&f);

    /* Iterator staleness across a discard. */
    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    CHECK(fill(&f, 4ul) != 0);
    {
        ledger89_iter *it;

        it = NULL;
        CHECK_EQ(ledger89_iter_open(f.l, 0ul, 0ul, &it), LEDGER89_OK);
        CHECK_EQ(ledger89_discard_before(f.l, 3ul), LEDGER89_OK);
        CHECK_EQ(ledger89_iter_next(it, &v), LEDGER89_ERR_STATE);
        ledger89_iter_close(it);
    }
    fx_close(&f);

    TEST_END;
}
