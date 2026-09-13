/* test_crash_rotate.c - rotation crash matrix over the model filesystem.
 *
 * Sweeps every call of each persistence primitive during one rotation and
 * asserts that recovery exposes the exact logical sequence with one active
 * segment. */

#include <string.h>

#include "test.h"

#include "crash_util.h"

static int fired_count;

static int run_case(const mfs *base, int op, int torn, int skip)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    int fired;

    mfs_init(&fs);
    mfs_clone(&fs, base);
    CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_OK);
    if (torn != 0)
    {
        mfs_arm_crash_torn(&fs, skip);
    }
    else
    {
        mfs_arm_crash(&fs, op, skip);
    }
    (void)ledger89_rotate(l);
    fired = mfs_crashed(&fs);
    if (fired == 0)
    {
        ledger89_close(l);
        mfs_destroy(&fs);
        return 0;
    }
    ++fired_count;
    mfs_crash(&fs);
    ledger89_close(l);
    CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 6ul);
    CHECK(cu_check_range(l, 1ul, 6ul) != 0);
    CHECK_EQ(cu_append(l, 7ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);
    mfs_destroy(&fs);
    return 1;
}

static void run_sweep(const mfs *base, int op, int torn)
{
    int skip;

    for (skip = 0;; ++skip)
    {
        if (run_case(base, op, torn, skip) == 0)
        {
            break;
        }
    }
}

int main(void)
{
    mfs base;
    led89_io io;
    ledger89 *l;

    /* Pre-state: sealed 1..3 plus active 4..6, all durable. */
    mfs_init(&base);
    CHECK_EQ(cu_open(&base, &io, &l, 0ul), LEDGER89_OK);
    CHECK(cu_fill(l, 3ul) != 0);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(l), LEDGER89_OK);
    CHECK(cu_fill_range(l, 4ul, 6ul) != 0);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);

    run_sweep(&base, MFS_OP_PWRITE, 0);
    run_sweep(&base, MFS_OP_PWRITE, 1);
    run_sweep(&base, MFS_OP_SYNC, 0);
    run_sweep(&base, MFS_OP_RENAME, 0);
    run_sweep(&base, MFS_OP_SYNC_DIR, 0);
    run_sweep(&base, MFS_OP_TRUNCATE, 0);
    CHECK(fired_count >= 8);

    mfs_destroy(&base);

    TEST_END;
}
