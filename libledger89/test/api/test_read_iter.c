/* test_read_iter.c - random read and iterator invalidation. */

#include "fixture.h"
#include "test.h"

static void fill(fx *f, unsigned long count)
{
    unsigned long i;

    for (i = 1ul; i <= count; ++i)
    {
        unsigned char b;
        ledger89_slice s;

        b = (unsigned char)('a' + (int)((i - 1ul) % 26ul));
        s.data = &b;
        s.size = 1u;
        CHECK_EQ(ledger89_appendv(f->l, &s, 1u, NULL), LEDGER89_OK);
    }
}

static void test_read(void)
{
    fx f;
    unsigned char buf[4];
    size_t size;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    fill(&f, 3u);
    rc = ledger89_read(f.l, test_u64(0), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_EGONE);
    rc = ledger89_read(f.l, test_u64(4), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_ENOENT);
    rc = ledger89_read(f.l, test_u64(1), NULL, 0u, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(size, 1u);
    rc = ledger89_read(f.l, test_u64(1), buf, 0u, &size);
    CHECK_EQ(rc, LEDGER89_ETOOSMALL);
    CHECK_EQ(size, 1u);
    rc = ledger89_read(f.l, test_u64(1), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'a');
    rc = ledger89_read(f.l, test_u64(1), buf, sizeof buf, NULL);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    fx_close(&f);
}

static void test_iteration(void)
{
    fx f;
    ledger89_iter it;
    ledger89_index idx;
    ledger89_slice s;
    unsigned char buf[4];
    size_t size;
    unsigned long seen;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    fill(&f, 3u);
    rc = ledger89_iter_init(&it, f.l, test_u64(0));
    CHECK_EQ(rc, LEDGER89_EGONE);
    rc = ledger89_iter_init(&it, f.l, test_u64(9));
    CHECK_EQ(rc, LEDGER89_ERANGE);
    rc = ledger89_iter_init(&it, f.l, test_u64(4));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, NULL, NULL, 0u, &size);
    CHECK_EQ(rc, LEDGER89_DONE);

    /* DONE is not permanent. */
    s.data = "d";
    s.size = 1u;
    rc = ledger89_appendv(f.l, &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(4));
    CHECK_EQ(buf[0], (unsigned char)'d');

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
        CHECK_U64(idx, test_u64(seen + 1ul));
        ++seen;
    }
    CHECK_EQ(rc, LEDGER89_DONE);
    CHECK_EQ(seen, 4ul);

    /* Capacity failure does not advance. */
    rc = ledger89_iter_init(&it, f.l, test_u64(1));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, &idx, buf, 0u, &size);
    CHECK_EQ(rc, LEDGER89_ETOOSMALL);
    CHECK_EQ(size, 1u);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(1));

    /* Ordinary append and sync do not invalidate. */
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    s.data = "e";
    rc = ledger89_appendv(f.l, &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(2));
    fx_close(&f);
}

static void test_truncate_invalidates(void)
{
    fx f;
    ledger89_iter it;
    ledger89_index idx;
    unsigned char buf[4];
    size_t size;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    fill(&f, 4u);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_init(&it, f.l, test_u64(1));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_truncate_from(f.l, test_u64(3));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_ESTALE);
    fx_close(&f);
}

static void test_prune_gone(void)
{
    fx f;
    ledger89_iter it;
    ledger89_index idx;
    ledger89_index actual;
    unsigned char buf[4];
    size_t size;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    fill(&f, 2u);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_rotate(f.l);
    CHECK_EQ(rc, LEDGER89_OK);
    fill(&f, 3u);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_prune_before(f.l, test_u64(3), &actual);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(actual, test_u64(3));
    rc = ledger89_iter_init(&it, f.l, test_u64(1));
    CHECK_EQ(rc, LEDGER89_EGONE);
    rc = ledger89_iter_init(&it, f.l, test_u64(3));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(3));
    rc = ledger89_read(f.l, test_u64(1), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_EGONE);
    fx_close(&f);
}

int main(void)
{
    test_read();
    test_iteration();
    test_truncate_invalidates();
    test_prune_gone();
    TEST_END;
}
