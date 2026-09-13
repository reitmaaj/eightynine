/* test_crash_append.c - append crash matrix over the model filesystem.
 *
 * Sweeps every call of each persistence primitive during one append and
 * asserts that recovery exposes the pre-state or the complete batch. */

#include <string.h>

#include "test.h"

#include "crash_util.h"

static int fired_count;

static int run_case(const mfs *base, int op, int torn, int skip)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_index last;
    int fired;
    int rc;

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
    rc = cu_append(l, 4ul);
    if (rc == LEDGER89_OK)
    {
        (void)ledger89_sync(l);
    }
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
    last = ledger89_last_index(l);
    CHECK(last == 3ul || last == 4ul);
    CHECK(cu_check_range(l, 1ul, last) != 0);
    CHECK_EQ(cu_append(l, last + 1ul), LEDGER89_OK);
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

    /* Pre-state: records 1..3 durable in the active segment. */
    mfs_init(&base);
    CHECK_EQ(cu_open(&base, &io, &l, 0ul), LEDGER89_OK);
    CHECK(cu_fill(l, 3ul) != 0);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);

    run_sweep(&base, MFS_OP_PWRITE, 0);
    run_sweep(&base, MFS_OP_PWRITE, 1);
    run_sweep(&base, MFS_OP_SYNC, 0);
    CHECK(fired_count >= 8);

    mfs_destroy(&base);

    TEST_END;
}
