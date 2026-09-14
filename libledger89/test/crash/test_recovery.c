/* test_recovery.c - targeted recovery outcomes over the model filesystem. */

#include <string.h>

#include "crash_util.h"
#include "test.h"

static void test_unsynced_tail_discarded(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_state st;

    mfs_init(&fs);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK(cu_fill(l, 3ul));
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    CHECK_EQ(cu_append(l, 4ul), LEDGER89_OK);
    mfs_crash(&fs);
    ledger89_close(l);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.first, test_u64(1));
    CHECK_U64(st.stable_end, test_u64(4));
    CHECK_U64(st.end, test_u64(4));
    CHECK(cu_check_range(l, 1ul, 3ul));
    ledger89_close(l);
    mfs_destroy(&fs);
}

static void test_complete_but_unsynced_batch(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_state st;

    mfs_init(&fs);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK(cu_fill(l, 3ul));
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    CHECK(cu_fill(l, 0ul));
    CHECK_EQ(cu_append(l, 4ul), LEDGER89_OK);
    CHECK_EQ(cu_append(l, 5ul), LEDGER89_OK);
    /* Power loss: both batches were complete in the page cache. */
    mfs_crash(&fs);
    ledger89_close(l);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.end, test_u64(4));
    CHECK_U64(st.stable_end, test_u64(4));
    ledger89_close(l);
    mfs_destroy(&fs);
}

static void test_framing_corruption_fails_open(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;

    mfs_init(&fs);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK(cu_fill(l, 3ul));
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    mfs_crash(&fs);
    ledger89_close(l);
    /* Flip the batch header magic inside the stable prefix. */
    CHECK(mfs_poke(&fs, "ledger/part.0000000000000001", 128u,
                   (unsigned char)'X') != 0);
    l = NULL;
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_ECORRUPT);
    CHECK(l == NULL);
    mfs_destroy(&fs);
}

static void test_payload_corruption_fails_read(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    unsigned char buf[2];
    size_t size;

    mfs_init(&fs);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK(cu_fill(l, 3ul));
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    mfs_crash(&fs);
    ledger89_close(l);
    /* Corrupt one payload byte: framing still validates at open. */
    CHECK(mfs_poke(&fs, "ledger/part.0000000000000001", 164u,
                   (unsigned char)'Z') != 0);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_read(l, test_u64(1), buf, sizeof buf, &size),
             LEDGER89_ECORRUPT);
    ledger89_close(l);
    mfs_destroy(&fs);
}

static void test_missing_manifest(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    int rc;

    mfs_init(&fs);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK(cu_fill(l, 2ul));
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    mfs_crash(&fs);
    ledger89_close(l);
    rc = io.unlink(io.ctx, "ledger/MANIFEST.0000000000000001");
    CHECK_EQ(rc, LEDGER89_OK);
    l = NULL;
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_ECORRUPT);
    mfs_destroy(&fs);
}

static void test_orphan_manifest_ignored(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    mfs_file *m;
    ledger89_state st;
    int rc;

    mfs_init(&fs);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK(cu_fill(l, 2ul));
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    mfs_crash(&fs);
    ledger89_close(l);

    m = mfs_find(&fs, "ledger/MANIFEST.0000000000000001");
    CHECK(m != NULL);
    if (m != NULL)
    {
        rc = mfs_put(&fs, "ledger/MANIFEST.0000000000000002", m->live_data,
                     m->live_size);
        CHECK(rc != 0);
    }
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.end, test_u64(3));
    ledger89_close(l);
    mfs_destroy(&fs);
}

static void test_idempotent_recovery(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_state st1;
    ledger89_state st2;

    mfs_init(&fs);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK(cu_fill(l, 2ul));
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    CHECK_EQ(cu_append(l, 3ul), LEDGER89_OK);
    mfs_crash(&fs);
    ledger89_close(l);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(l, &st1), LEDGER89_OK);
    ledger89_close(l);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(l, &st2), LEDGER89_OK);
    CHECK_U64(st1.end, st2.end);
    CHECK_U64(st1.stable_end, st2.stable_end);
    CHECK_U64(st1.first, st2.first);
    ledger89_close(l);
    mfs_destroy(&fs);
}

int main(void)
{
    test_unsynced_tail_discarded();
    test_complete_but_unsynced_batch();
    test_framing_corruption_fails_open();
    test_payload_corruption_fails_read();
    test_missing_manifest();
    test_orphan_manifest_ignored();
    test_idempotent_recovery();
    TEST_END;
}
