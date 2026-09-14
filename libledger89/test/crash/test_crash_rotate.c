/* test_crash_rotate.c - crash sweep over sealing and rotation. */

#include "crash_util.h"
#include "test.h"

static void check_recovered(mfs *fs, led89_io *io)
{
    ledger89 *l;
    ledger89_state st;

    if (cu_reopen(fs, io, &l, &st) == 0)
    {
        CHECK(0);
        return;
    }
    CHECK_U64(st.first, test_u64(1));
    CHECK_U64(st.end, test_u64(4));
    CHECK_U64(st.stable_end, test_u64(4));
    CHECK_EQ(st.revision.lo, 0u);
    CHECK(cu_check_range(l, 1ul, 3ul));
    CHECK_EQ(cu_append(l, 4ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    ledger89_close(l);
}

static void sweep(int op, int torn)
{
    int skip;

    for (skip = 0; skip < 1000; ++skip)
    {
        mfs fs;
        led89_io io;
        ledger89 *l;

        mfs_init(&fs);
        CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
        CHECK(cu_fill(l, 3ul));
        CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
        if (torn != 0)
        {
            mfs_arm_crash_torn(&fs, skip);
        }
        else
        {
            mfs_arm_crash(&fs, op, skip);
        }
        (void)ledger89_rotate(l);
        if (mfs_crashed(&fs) == 0)
        {
            ledger89_close(l);
            mfs_destroy(&fs);
            return;
        }
        mfs_crash(&fs);
        ledger89_close(l);
        check_recovered(&fs, &io);
        mfs_destroy(&fs);
    }
    CHECK(0);
}

int main(void)
{
    sweep(MFS_OP_PWRITE, 0);
    sweep(MFS_OP_PWRITE, 1);
    sweep(MFS_OP_SYNC, 0);
    sweep(MFS_OP_RENAME, 0);
    sweep(MFS_OP_UNLINK, 0);
    sweep(MFS_OP_SYNC_DIR, 0);
    sweep(MFS_OP_TRUNCATE, 0);
    sweep(MFS_OP_OPEN, 0);
    TEST_END;
}
