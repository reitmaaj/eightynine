/* test_read_iter.c - random read and iterator invalidation. */

#include <string.h>

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

static void test_iteration_multi_record_batch(void)
{
    fx f;
    ledger89_iter it;
    ledger89_index idx;
    unsigned char buf[8];
    size_t size;
    unsigned long i;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    {
        ledger89_slice slices[4];
        static const char *texts[4] = {"a", "bb", "ccc", "dddd"};

        for (i = 0ul; i < 4ul; ++i)
        {
            slices[i].data = texts[i];
            slices[i].size = strlen(texts[i]);
        }
        CHECK_EQ(ledger89_appendv(f.l, slices, 4u, NULL), LEDGER89_OK);
    }
    {
        ledger89_slice slices[2];

        slices[0].data = NULL;
        slices[0].size = 0u;
        slices[1].data = "z";
        slices[1].size = 1u;
        CHECK_EQ(ledger89_appendv(f.l, slices, 2u, NULL), LEDGER89_OK);
    }

    /* Payload iteration across two batches, including a zero-length record. */
    rc = ledger89_iter_init(&it, f.l, test_u64(1));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(1));
    CHECK_EQ(size, 1u);
    CHECK_EQ(buf[0], (unsigned char)'a');
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(2));
    CHECK_EQ(size, 2u);
    CHECK(memcmp(buf, "bb", 2u) == 0);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(3));
    CHECK_EQ(size, 3u);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(4));
    CHECK_EQ(size, 4u);
    rc = ledger89_iter_next(&it, &idx, NULL, 0u, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(5));
    CHECK_EQ(size, 0u);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(6));
    CHECK_EQ(size, 1u);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_DONE);

    /* Capacity failure keeps the cursor on the same record. */
    rc = ledger89_iter_init(&it, f.l, test_u64(2));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, &idx, buf, 1u, &size);
    CHECK_EQ(rc, LEDGER89_ETOOSMALL);
    CHECK_EQ(size, 2u);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(2));
    CHECK_EQ(size, 2u);

    /* Mid-batch start after a reopen. */
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    rc = ledger89_iter_init(&it, f.l, test_u64(3));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, &idx, NULL, 0u, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(3));
    CHECK_EQ(size, 3u);
    rc = ledger89_iter_next(&it, &idx, NULL, 0u, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(4));
    CHECK_EQ(size, 4u);

    /* Rotation does not invalidate; iteration crosses into the new part. */
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(f.l), LEDGER89_OK);
    {
        ledger89_slice s;

        s.data = "e";
        s.size = 1u;
        CHECK_EQ(ledger89_appendv(f.l, &s, 1u, NULL), LEDGER89_OK);
    }
    rc = ledger89_iter_next(&it, &idx, NULL, 0u, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(5));
    CHECK_EQ(size, 0u);
    rc = ledger89_iter_next(&it, &idx, NULL, 0u, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(6));
    CHECK_EQ(size, 1u);
    rc = ledger89_iter_next(&it, &idx, NULL, 0u, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(7));
    CHECK_EQ(size, 1u);
    rc = ledger89_iter_next(&it, &idx, NULL, 0u, &size);
    CHECK_EQ(rc, LEDGER89_DONE);

    fx_close(&f);
}

static void append_pair(fx *f, const char *a, const char *b)
{
    ledger89_slice slices[2];

    slices[0].data = a;
    slices[0].size = strlen(a);
    slices[1].data = b;
    slices[1].size = strlen(b);
    CHECK_EQ(ledger89_appendv(f->l, slices, 2u, NULL), LEDGER89_OK);
}

static void test_iteration_cursor_prune(void)
{
    fx f;
    ledger89_iter it;
    ledger89_index idx;
    ledger89_index actual;
    unsigned char buf[4];
    size_t size;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    append_pair(&f, "a", "b");
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(f.l), LEDGER89_OK);
    append_pair(&f, "c", "d");
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(f.l), LEDGER89_OK);
    append_pair(&f, "e", "f");
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);

    /* Cursor shifted onto a later batch after pruning an earlier one. */
    rc = ledger89_iter_init(&it, f.l, test_u64(3));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(3));
    rc = ledger89_prune_before(f.l, test_u64(3), &actual);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(actual, test_u64(3));
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(4));
    CHECK_EQ(buf[0], (unsigned char)'d');

    /* Cursor entry falls out of range after pruning its own batch. */
    rc = ledger89_iter_init(&it, f.l, test_u64(5));
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(5));
    rc = ledger89_prune_before(f.l, test_u64(5), &actual);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(actual, test_u64(5));
    rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(idx, test_u64(6));
    CHECK_EQ(buf[0], (unsigned char)'f');

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
    test_iteration_multi_record_batch();
    test_iteration_cursor_prune();
    test_truncate_invalidates();
    test_prune_gone();
    TEST_END;
}
