/* test_stress_retained.c - full lifecycle on a retained multi-part ledger.
 *
 * Builds a ledger beyond unit-test scale, then closes, reopens, reads,
 * iterates, verifies, keeps writing, and reopens again. */

#include <string.h>

#include "fixture.h"
#include "test.h"

#define RETAINED_RECORDS 20000ul
#define RETAINED_PARTS 16ul
#define RETAINED_PAYLOAD 64u
#define RETAINED_MORE 100ul

static void fill_range(fx *f, unsigned long first, unsigned long last)
{
    unsigned long i;
    unsigned long chunk;

    chunk = RETAINED_RECORDS / RETAINED_PARTS;
    for (i = first; i <= last; ++i)
    {
        unsigned char payload[RETAINED_PAYLOAD];
        ledger89_slice s;

        memset(payload, (int)(i & 0xFFul), RETAINED_PAYLOAD);
        s.data = payload;
        s.size = RETAINED_PAYLOAD;
        CHECK_EQ(ledger89_appendv(f->l, &s, 1u, NULL), LEDGER89_OK);
        if ((i % chunk) == 0ul && i != 0ul && i <= RETAINED_RECORDS)
        {
            CHECK_EQ(ledger89_sync(f->l, NULL), LEDGER89_OK);
            CHECK_EQ(ledger89_rotate(f->l), LEDGER89_OK);
        }
    }
}

int main(void)
{
    fx f;
    ledger89_state st;
    ledger89_iter it;
    ledger89_index idx;
    unsigned char buf[RETAINED_PAYLOAD];
    size_t size;
    unsigned long seen;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    fill_range(&f, 1ul, RETAINED_RECORDS);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);

    /* Cold reopen: state, sampled reads, full iteration, and verification. */
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(f.l, &st), LEDGER89_OK);
    CHECK_U64(st.first, test_u64(1));
    CHECK_U64(st.stable_end, test_u64(RETAINED_RECORDS + 1ul));
    CHECK_U64(st.end, test_u64(RETAINED_RECORDS + 1ul));
    CHECK(fx_count_parts(&f) > 1);

    rc = ledger89_read(f.l, test_u64(1), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(size, (size_t)RETAINED_PAYLOAD);
    CHECK_EQ(buf[0], (unsigned char)(1u & 0xFFu));
    rc = ledger89_read(f.l, test_u64(RETAINED_RECORDS), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)(RETAINED_RECORDS & 0xFFul));

    rc = ledger89_iter_init(&it, f.l, test_u64(1));
    CHECK_EQ(rc, LEDGER89_OK);
    seen = 0ul;
    for (;;)
    {
        rc = ledger89_iter_next(&it, &idx, NULL, 0u, &size);
        if (rc != LEDGER89_OK)
        {
            break;
        }
        ++seen;
    }
    CHECK_EQ(rc, LEDGER89_DONE);
    CHECK_EQ(seen, RETAINED_RECORDS);

    CHECK_EQ(ledger89_verify(f.l), LEDGER89_OK);

    /* Keep writing and reopen again. */
    fill_range(&f, RETAINED_RECORDS + 1ul, RETAINED_RECORDS + RETAINED_MORE);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    fx_close(&f);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(f.l, &st), LEDGER89_OK);
    CHECK_U64(st.end, test_u64(RETAINED_RECORDS + RETAINED_MORE + 1ul));
    rc = ledger89_read(f.l, test_u64(RETAINED_RECORDS + RETAINED_MORE), buf,
                       sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(buf[0],
             (unsigned char)((RETAINED_RECORDS + RETAINED_MORE) & 0xFFul));
    CHECK_EQ(ledger89_verify(f.l), LEDGER89_OK);
    fx_close(&f);
    TEST_END;
}
