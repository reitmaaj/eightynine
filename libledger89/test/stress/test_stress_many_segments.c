/* test_stress_many_segments.c - many sealed parts and recovery across them. */

#include "fixture.h"
#include "test.h"

#ifndef MANY_RECORDS
#define MANY_RECORDS 200ul
#endif

int main(void)
{
    fx f;
    ledger89_slice s;
    ledger89_state st;
    ledger89_iter it;
    ledger89_index idx;
    unsigned char buf[4];
    size_t size;
    unsigned long i;
    unsigned long seen;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    for (i = 1ul; i <= MANY_RECORDS; ++i)
    {
        unsigned char b;

        b = (unsigned char)('a' + (int)(i % 26ul));
        s.data = &b;
        s.size = 1u;
        CHECK_EQ(ledger89_appendv(f.l, &s, 1u, NULL), LEDGER89_OK);
        CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
        CHECK_EQ(ledger89_rotate(f.l), LEDGER89_OK);
    }
    CHECK_EQ(ledger89_get_state(f.l, &st), LEDGER89_OK);
    CHECK_U64(st.first, test_u64(1));
    CHECK_U64(st.end, test_u64(MANY_RECORDS + 1ul));
    CHECK_EQ(fx_count_parts(&f), (int)MANY_RECORDS + 1);

    rc = ledger89_iter_init(&it, f.l, test_u64(1));
    CHECK_EQ(rc, LEDGER89_OK);
    seen = 0ul;
    for (;;)
    {
        rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
        if (rc != LEDGER89_OK)
        {
            break;
        }
        ++seen;
    }
    CHECK_EQ(rc, LEDGER89_DONE);
    CHECK_EQ(seen, MANY_RECORDS);
    fx_close(&f);

    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    rc = ledger89_read(f.l, test_u64(MANY_RECORDS / 2ul), buf, sizeof buf,
                       &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(size, 1u);
    fx_close(&f);
    TEST_END;
}
