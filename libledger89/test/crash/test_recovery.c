/* test_recovery.c - targeted recovery outcomes over the model filesystem. */

#include <string.h>

#include "crash_util.h"
#include "test.h"

#define MANIFEST_PREFIX "MANIFEST."

static int count_manifests(led89_io *io)
{
    led89_dir *dir;
    char name[32];
    int done;
    int count;
    int rc;

    dir = NULL;
    rc = io->list_open(io->ctx, "ledger", &dir);
    if (rc != LEDGER89_OK)
    {
        return -1;
    }
    done = 0;
    count = 0;
    while (done == 0)
    {
        rc = io->list_next(io->ctx, dir, name, sizeof name, &done);
        if (rc != LEDGER89_OK)
        {
            io->list_close(io->ctx, dir);
            return -1;
        }
        if (done != 0)
        {
            break;
        }
        if (strncmp(name, MANIFEST_PREFIX, sizeof(MANIFEST_PREFIX) - 1u) == 0)
        {
            ++count;
        }
    }
    io->list_close(io->ctx, dir);
    return count;
}

static void build_three_manifests(mfs *fs, led89_io *io, ledger89 **l)
{
    CHECK_EQ(cu_open(fs, io, l), LEDGER89_OK);
    CHECK(cu_fill(*l, 1ul));
    CHECK_EQ(ledger89_sync(*l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(*l), LEDGER89_OK);
    CHECK_EQ(cu_append(*l, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(*l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(*l), LEDGER89_OK);
    CHECK_EQ(cu_append(*l, 3ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(*l, NULL), LEDGER89_OK);
}

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

    /* Open establishes structural recoverability, not payload validity. */
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);

    /* A size-only read does not read or checksum the payload. */
    size = 0u;
    CHECK_EQ(ledger89_read(l, test_u64(1), NULL, 0u, &size), LEDGER89_OK);
    CHECK_EQ(size, 1u);

    /* Reading the payload verifies its checksum and detects the corruption. */
    size = 0u;
    CHECK_EQ(ledger89_read(l, test_u64(1), buf, sizeof buf, &size),
             LEDGER89_ECORRUPT);
    CHECK_EQ(size, 1u);

    /* Exhaustive verification also detects it. */
    CHECK_EQ(ledger89_verify(l), LEDGER89_ECORRUPT);

    /* Iteration that copies the payload reports the same corruption. */
    {
        ledger89_iter it;
        ledger89_index idx;
        size_t iter_size;

        CHECK_EQ(ledger89_iter_init(&it, l, test_u64(1)), LEDGER89_OK);
        CHECK_EQ(ledger89_iter_next(&it, &idx, buf, sizeof buf, &iter_size),
                 LEDGER89_ECORRUPT);
    }
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

static void test_manifest_gc_on_recovery(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    size_t size;

    mfs_init(&fs);
    build_three_manifests(&fs, &io, &l);
    CHECK_EQ(count_manifests(&io), 3);
    mfs_crash(&fs);
    ledger89_close(l);
    l = NULL;
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    /* Writable recovery retains only the generation CURRENT names. */
    CHECK_EQ(count_manifests(&io), 1);
    size = 0u;
    CHECK_EQ(ledger89_read(l, test_u64(1), NULL, 0u, &size), LEDGER89_OK);
    CHECK_EQ(ledger89_verify(l), LEDGER89_OK);
    ledger89_close(l);
    mfs_destroy(&fs);
}

static void test_manifest_gc_unlink_failure(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    size_t size;

    mfs_init(&fs);
    build_three_manifests(&fs, &io, &l);
    mfs_crash(&fs);
    ledger89_close(l);
    l = NULL;
    /* Cleanup is best-effort: a failed unlink must not fail the open or
     * remove the manifest CURRENT names. */
    mfs_fail_at(&fs, MFS_OP_UNLINK, 0);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK(mfs_exists(&fs, "ledger/MANIFEST.0000000000000003") != 0);
    size = 0u;
    CHECK_EQ(ledger89_read(l, test_u64(1), NULL, 0u, &size), LEDGER89_OK);
    CHECK_EQ(ledger89_verify(l), LEDGER89_OK);
    ledger89_close(l);
    mfs_destroy(&fs);
}

static void test_iteration_open_failure(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_iter it;
    ledger89_index idx;
    unsigned char buf[4];
    size_t size;

    mfs_init(&fs);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK(cu_fill(l, 2ul));
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(l), LEDGER89_OK);
    mfs_crash(&fs);
    ledger89_close(l);
    l = NULL;
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    /* Sealed parts are closed after recovery; the first open fails. */
    mfs_fail_at(&fs, MFS_OP_OPEN, 0);
    CHECK_EQ(ledger89_iter_init(&it, l, test_u64(1)), LEDGER89_OK);
    CHECK_EQ(ledger89_iter_next(&it, &idx, buf, sizeof buf, &size),
             LEDGER89_EIO);
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
    test_manifest_gc_on_recovery();
    test_manifest_gc_unlink_failure();
    test_iteration_open_failure();
    test_idempotent_recovery();
    TEST_END;
}
