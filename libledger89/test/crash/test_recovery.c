/* test_recovery.c - R01..R08: crash recovery, torn tails, and batch
 * atomicity over the deterministic model filesystem. */

#include <string.h>

#include "test.h"

#include "model_fs.h"

static int open_model(mfs *fs, led89_io *io, ledger89 **l)
{
    ledger89_config cfg;

    mfs_bind(io, fs);
    memset(&cfg, 0, sizeof cfg);
    cfg.path = "ledger";
    cfg.max_segment_bytes = 4096ul;
    cfg.max_segment_records = 0ul;
    *l = NULL;
    return led89_open_io(l, &cfg, io);
}

static int append_one(ledger89 *l, ledger89_index index, unsigned long tag)
{
    ledger89_record r;
    unsigned char data[1];

    data[0] = 'x';
    r.index = index;
    r.tag = tag;
    r.data = data;
    r.size = 1u;
    return ledger89_append(l, &r, 1u);
}

static int read_one(ledger89 *l, ledger89_index index)
{
    ledger89_view v;

    return ledger89_read(l, index, &v);
}

int main(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    mfs_file *f;

    /* R02: a process crash keeps the page cache, so an unsynced append
     * survives. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(append_one(l, 1ul, 1ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    CHECK_EQ(append_one(l, 2ul, 2ul), LEDGER89_OK);
    ledger89_close(l);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 2ul);
    ledger89_close(l);
    mfs_destroy(&fs);

    /* R01: power loss discards the unsynced append. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(append_one(l, 1ul, 1ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    CHECK_EQ(append_one(l, 2ul, 2ul), LEDGER89_OK);
    ledger89_close(l);
    mfs_crash(&fs);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 1ul);
    CHECK_EQ(read_one(l, 1ul), LEDGER89_OK);
    CHECK_EQ(read_one(l, 2ul), LEDGER89_ERR_NOTFOUND);
    /* R08: append after recovery continues contiguously and syncs. */
    CHECK_EQ(append_one(l, 2ul, 22ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);
    mfs_crash(&fs);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 2ul);
    ledger89_close(l);
    mfs_destroy(&fs);

    /* R03: a torn append never becomes visible; recovery truncates it. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(append_one(l, 1ul, 1ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    fs.torn_bytes = 10;
    CHECK_EQ(append_one(l, 2ul, 2ul), LEDGER89_ERR_IO);
    CHECK_EQ(append_one(l, 2ul, 2ul), LEDGER89_ERR_FAULTED);
    ledger89_close(l);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 1ul);
    CHECK_EQ(read_one(l, 2ul), LEDGER89_ERR_NOTFOUND);
    CHECK_EQ(mfs_live_size(&fs, "ledger/active.seg"), 117u);
    ledger89_close(l);
    mfs_destroy(&fs);

    /* R04: arbitrary bytes after the last batch are a torn tail. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(append_one(l, 1ul, 1ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);
    CHECK(mfs_insert(&fs, "ledger/active.seg",
                     mfs_live_size(&fs, "ledger/active.seg"),
                     "\xFF\xFF\xFF\xFF", 4u) != 0);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 1ul);
    CHECK_EQ(mfs_live_size(&fs, "ledger/active.seg"), 117u);
    ledger89_close(l);
    mfs_destroy(&fs);

    /* R05: arbitrary bytes between complete batches are corruption. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(append_one(l, 1ul, 1ul), LEDGER89_OK);
    CHECK_EQ(append_one(l, 2ul, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);
    CHECK(mfs_insert(&fs, "ledger/active.seg", 117u, "\xFF\xFF\xFF\xFF", 4u) !=
          0);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_ERR_CORRUPT);
    CHECK(l == NULL);
    mfs_destroy(&fs);

    /* R06: a torn active header is recreated as an empty ledger. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(append_one(l, 1ul, 1ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);
    f = mfs_find(&fs, "ledger/active.seg");
    CHECK(f != NULL);
    if (f != NULL)
    {
        f->live_size = 10u;
    }
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 0ul);
    CHECK_EQ(mfs_live_size(&fs, "ledger/active.seg"), 32u);
    ledger89_close(l);
    mfs_destroy(&fs);

    /* R07: recovery is idempotent. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(append_one(l, 1ul, 1ul), LEDGER89_OK);
    CHECK_EQ(append_one(l, 2ul, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);
    {
        int i;

        for (i = 0; i < 3; ++i)
        {
            CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
            CHECK_EQ(ledger89_last_index(l), 2ul);
            ledger89_close(l);
        }
    }
    mfs_destroy(&fs);

    TEST_END;
}
