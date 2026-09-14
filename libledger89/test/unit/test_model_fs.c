/* test_model_fs.c - MR01..MR04: model filesystem durability contract. */

#include <string.h>

#include "test.h"

#include "model_fs.h"

int main(void)
{
    mfs fs;
    led89_io io;
    led89_fd fd;
    unsigned char buf[8];
    led89_u64 size;

    mfs_init(&fs);
    mfs_bind(&io, &fs);

    /* MR01: created, written, file-synced, but no directory sync: the name
     * is lost on power loss. */
    CHECK_EQ(io.open(io.ctx, "ledger/a",
                     LED89_OPEN_READ | LED89_OPEN_WRITE | LED89_OPEN_CREATE,
                     &fd),
             LEDGER89_OK);
    CHECK_EQ(io.pwrite(io.ctx, fd, "abc", 3u, 0u), LEDGER89_OK);
    CHECK_EQ(io.sync(io.ctx, fd), LEDGER89_OK);
    CHECK_EQ(io.size(io.ctx, fd, &size), LEDGER89_OK);
    CHECK_EQ(size, 3u);
    mfs_crash(&fs);
    CHECK_EQ(mfs_exists(&fs, "ledger/a"), 0);

    /* MR02: with a directory sync the name and bytes survive. */
    CHECK_EQ(io.open(io.ctx, "ledger/a",
                     LED89_OPEN_READ | LED89_OPEN_WRITE | LED89_OPEN_CREATE,
                     &fd),
             LEDGER89_OK);
    CHECK_EQ(io.pwrite(io.ctx, fd, "abc", 3u, 0u), LEDGER89_OK);
    CHECK_EQ(io.sync(io.ctx, fd), LEDGER89_OK);
    CHECK_EQ(io.sync_dir(io.ctx, "ledger"), LEDGER89_OK);
    mfs_crash(&fs);
    CHECK_EQ(mfs_exists(&fs, "ledger/a"), 1);
    CHECK_EQ(mfs_live_size(&fs, "ledger/a"), 3u);
    CHECK_EQ(io.open(io.ctx, "ledger/a", LED89_OPEN_READ, &fd), LEDGER89_OK);
    CHECK_EQ(io.pread(io.ctx, fd, buf, 3u, 0u), LEDGER89_OK);
    CHECK(memcmp(buf, "abc", 3u) == 0);
    CHECK_EQ(io.pread(io.ctx, fd, buf, 4u, 0u), LEDGER89_EIO);

    /* MR03: a torn write persists only its prefix and fails. */
    fs.torn_bytes = 2;
    CHECK_EQ(io.pwrite(io.ctx, fd, "XYZ", 3u, 3u), LEDGER89_EIO);
    CHECK_EQ(mfs_live_size(&fs, "ledger/a"), 5u);
    CHECK_EQ(io.pread(io.ctx, fd, buf, 2u, 3u), LEDGER89_OK);
    CHECK(memcmp(buf, "XY", 2u) == 0);

    /* MR04: a rename without a directory sync is rolled back. */
    CHECK_EQ(io.rename(io.ctx, "ledger/a", "ledger/b"), LEDGER89_OK);
    CHECK_EQ(mfs_exists(&fs, "ledger/b"), 1);
    mfs_crash(&fs);
    CHECK_EQ(mfs_exists(&fs, "ledger/a"), 1);
    CHECK_EQ(mfs_exists(&fs, "ledger/b"), 0);

    /* Direct setup and mutation helpers. */
    CHECK(mfs_put(&fs, "ledger/c", "hello", 5u) != 0);
    CHECK_EQ(mfs_live_size(&fs, "ledger/c"), 5u);
    CHECK(mfs_insert(&fs, "ledger/c", 0u, ">", 1u) != 0);
    CHECK_EQ(mfs_live_size(&fs, "ledger/c"), 6u);
    CHECK(mfs_poke(&fs, "ledger/c", 0u, (unsigned char)'!') != 0);
    CHECK_EQ(io.open(io.ctx, "ledger/c", LED89_OPEN_READ, &fd), LEDGER89_OK);
    CHECK_EQ(io.pread(io.ctx, fd, buf, 1u, 0u), LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'!');

    mfs_destroy(&fs);

    TEST_END;
}
