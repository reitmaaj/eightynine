/* test_crash_append.c - crash sweeps over append and sync. */

#include "crash_util.h"
#include "test.h"

static int reopen_ok(mfs *fs, led89_io *io, ledger89 **l, ledger89_state *st)
{
    int rc;

    *l = NULL;
    rc = cu_open(fs, io, l);
    if (rc != LEDGER89_OK)
    {
        return 0;
    }
    if (ledger89_get_state(*l, st) != LEDGER89_OK)
    {
        return 0;
    }
    return 1;
}

static void check_recovered(mfs *fs, led89_io *io)
{
    ledger89 *l;
    ledger89_state st;
    unsigned long end;

    if (reopen_ok(fs, io, &l, &st) == 0)
    {
        CHECK(0);
        return;
    }
    CHECK_U64(st.first, test_u64(1));
    end = st.end.lo;
    CHECK(end >= 4ul);
    CHECK(end <= 5ul);
    CHECK_U64(st.stable_end, st.end);
    if (end == 5ul)
    {
        CHECK(cu_check_range(l, 1ul, 4ul));
    }
    else
    {
        CHECK(cu_check_range(l, 1ul, 3ul));
    }
    /* The recovered ledger must remain usable. */
    CHECK_EQ(cu_append(l, end), LEDGER89_OK);
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
        (void)cu_append(l, 4ul);
        (void)ledger89_sync(l, NULL);
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
    sweep(MFS_OP_TRUNCATE, 0);
    sweep(MFS_OP_RENAME, 0);
    sweep(MFS_OP_SYNC_DIR, 0);
    TEST_END;
}
