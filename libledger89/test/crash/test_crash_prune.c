/* test_crash_prune.c - crash sweep over prefix pruning. */

#include "crash_util.h"
#include "test.h"

static void prestate(mfs *fs, led89_io *io, ledger89 **l)
{
    CHECK_EQ(cu_open(fs, io, l), LEDGER89_OK);
    CHECK(cu_fill_range(*l, 1ul, 3ul));
    CHECK_EQ(ledger89_sync(*l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(*l), LEDGER89_OK);
    CHECK(cu_fill_range(*l, 4ul, 6ul));
    CHECK_EQ(ledger89_sync(*l, NULL), LEDGER89_OK);
}

static void check_recovered(mfs *fs, led89_io *io)
{
    ledger89 *l;
    ledger89_state st;

    if (cu_reopen(fs, io, &l, &st) == 0)
    {
        CHECK(0);
        return;
    }
    CHECK_U64(st.end, test_u64(7));
    CHECK_U64(st.stable_end, test_u64(7));
    CHECK_EQ(st.revision.lo, 0u);
    if (st.first.lo == 1u)
    {
        CHECK(cu_check_range(l, 1ul, 6ul));
        CHECK_EQ(cu_append(l, 7ul), LEDGER89_OK);
    }
    else
    {
        CHECK_U64(st.first, test_u64(4));
        CHECK(cu_check_range(l, 4ul, 6ul));
        CHECK_EQ(cu_append(l, 7ul), LEDGER89_OK);
    }
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
        ledger89_index actual;

        mfs_init(&fs);
        prestate(&fs, &io, &l);
        if (torn != 0)
        {
            mfs_arm_crash_torn(&fs, skip);
        }
        else
        {
            mfs_arm_crash(&fs, op, skip);
        }
        (void)ledger89_prune_before(l, test_u64(4), &actual);
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
    sweep(MFS_OP_OPEN, 0);
    TEST_END;
}
