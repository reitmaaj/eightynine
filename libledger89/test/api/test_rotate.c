/* test_rotate.c - explicit rotation and cross-part access. */

#include "fixture.h"
#include "test.h"

static void fill(fx *f, unsigned long count)
{
    unsigned long i;

    for (i = 0ul; i < count; ++i)
    {
        unsigned char b;
        ledger89_slice s;

        b = (unsigned char)('a' + (int)(i % 26ul));
        s.data = &b;
        s.size = 1u;
        CHECK_EQ(ledger89_appendv(f->l, &s, 1u, NULL), LEDGER89_OK);
    }
}

int main(void)
{
    fx f;
    ledger89_state st;
    ledger89_iter it;
    ledger89_index idx;
    unsigned char buf[4];
    size_t size;
    unsigned long seen;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    CHECK_EQ(fx_count_parts(&f), 1);

    /* Rotating an empty active part is a no-op. */
    rc = ledger89_rotate(f.l);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(fx_count_parts(&f), 1);

    fill(&f, 2u);
    rc = ledger89_rotate(f.l);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(fx_count_parts(&f), 2);
    CHECK_EQ(fx_count_manifests(&f), 2);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.first, test_u64(1));
    CHECK_U64(st.stable_end, test_u64(3));
    CHECK_U64(st.end, test_u64(3));

    fill(&f, 2u);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_rotate(f.l);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(fx_count_parts(&f), 3);
    CHECK_EQ(fx_count_manifests(&f), 3);
    rc = ledger89_rotate(f.l);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(fx_count_parts(&f), 3);
    fill(&f, 1u);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);

    /* Cross-part random access. */
    rc = ledger89_read(f.l, test_u64(1), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'a');
    rc = ledger89_read(f.l, test_u64(3), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'a');
    rc = ledger89_read(f.l, test_u64(5), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'a');

    /* Cross-part iteration. */
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
    CHECK_EQ(seen, 5ul);

    /* Rotation is durable. */
    fx_close(&f);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.end, test_u64(6));
    CHECK_EQ(fx_count_parts(&f), 3);
    /* Writable recovery retains only the manifest CURRENT names. */
    CHECK_EQ(fx_count_manifests(&f), 1);
    fx_close(&f);
    TEST_END;
}
