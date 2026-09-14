/* test_fault_matrix.c - single-call I/O failures poison mutating handles and
 * every reopen recovers a legal state. */

#include "crash_util.h"
#include "test.h"

typedef int (*mut_fn)(ledger89 *l);

enum kind
{
    KIND_APPEND = 0,
    KIND_TRUNCATE = 1,
    KIND_PRUNE = 2,
    KIND_ROTATE = 3
};

static int do_append(ledger89 *l)
{
    if (cu_append(l, 7ul) != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    return ledger89_sync(l, NULL);
}

static int do_truncate(ledger89 *l)
{
    return ledger89_truncate_from(l, test_u64(2));
}

static int do_prune(ledger89 *l)
{
    ledger89_index actual;

    return ledger89_prune_before(l, test_u64(4), &actual);
}

static int do_rotate(ledger89 *l)
{
    return ledger89_rotate(l);
}

static void prestate(mfs *fs, led89_io *io, ledger89 **l)
{
    CHECK_EQ(cu_open(fs, io, l), LEDGER89_OK);
    CHECK(cu_fill_range(*l, 1ul, 3ul));
    CHECK_EQ(ledger89_sync(*l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(*l), LEDGER89_OK);
    CHECK(cu_fill_range(*l, 4ul, 6ul));
    CHECK_EQ(ledger89_sync(*l, NULL), LEDGER89_OK);
}

static void validate(int kind, ledger89 *l, const ledger89_state *st)
{
    CHECK_U64(st->stable_end, st->end);
    if (kind == KIND_APPEND)
    {
        CHECK(st->end.lo >= 7u && st->end.lo <= 8u);
        if (st->end.lo == 8u)
        {
            CHECK(cu_check_range(l, 1ul, 7ul));
        }
        else
        {
            CHECK(cu_check_range(l, 1ul, 6ul));
        }
    }
    else if (kind == KIND_TRUNCATE)
    {
        if (st->revision.lo == 0u)
        {
            CHECK_U64(st->end, test_u64(7));
            CHECK(cu_check_range(l, 1ul, 6ul));
        }
        else
        {
            CHECK_EQ(st->revision.lo, 1u);
            CHECK_U64(st->end, test_u64(2));
            CHECK(cu_check_range(l, 1ul, 1ul));
        }
    }
    else if (kind == KIND_PRUNE)
    {
        CHECK_U64(st->end, test_u64(7));
        CHECK_EQ(st->revision.lo, 0u);
        CHECK(st->first.lo == 1u || st->first.lo == 4u);
        CHECK(cu_check_range(l, st->first.lo, 6ul));
    }
    else
    {
        CHECK_U64(st->first, test_u64(1));
        CHECK_U64(st->end, test_u64(7));
        CHECK_EQ(st->revision.lo, 0u);
        CHECK(cu_check_range(l, 1ul, 6ul));
    }
    CHECK_EQ(cu_append(l, 9ul), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
}

static void sweep(int kind, int op, mut_fn fn)
{
    int skip;

    for (skip = 0; skip < 200; ++skip)
    {
        mfs fs;
        led89_io io;
        ledger89 *l;
        ledger89_state st;

        mfs_init(&fs);
        prestate(&fs, &io, &l);
        mfs_fail_at(&fs, op, skip);
        {
            int oprc;

            oprc = fn(l);
            if (mfs_fail_fired(&fs) == 0)
            {
                CHECK_EQ(oprc, LEDGER89_OK);
                ledger89_close(l);
                mfs_destroy(&fs);
                return;
            }
            if (oprc == LEDGER89_OK)
            {
                /* A harmless failure such as post-publication garbage
                 * collection leaves the handle usable. */
                CHECK_EQ(cu_append(l, 9ul), LEDGER89_OK);
            }
            else
            {
                CHECK_EQ(cu_append(l, 9ul), LEDGER89_EPOISONED);
            }
        }
        ledger89_close(l);
        if (cu_reopen(&fs, &io, &l, &st) == 0)
        {
            CHECK(0);
            mfs_destroy(&fs);
            return;
        }
        validate(kind, l, &st);
        ledger89_close(l);
        mfs_destroy(&fs);
    }
    CHECK(0);
}

static void sweep_all(int kind, mut_fn fn)
{
    sweep(kind, MFS_OP_PWRITE, fn);
    sweep(kind, MFS_OP_SYNC, fn);
    sweep(kind, MFS_OP_RENAME, fn);
    sweep(kind, MFS_OP_UNLINK, fn);
    sweep(kind, MFS_OP_TRUNCATE, fn);
    sweep(kind, MFS_OP_OPEN, fn);
    sweep(kind, MFS_OP_SYNC_DIR, fn);
    sweep(kind, MFS_OP_LIST_OPEN, fn);
    sweep(kind, MFS_OP_LIST_NEXT, fn);
    sweep(kind, MFS_OP_LIST_CLOSE, fn);
    sweep(kind, MFS_OP_PREAD, fn);
}

int main(void)
{
    sweep_all(KIND_APPEND, do_append);
    sweep_all(KIND_TRUNCATE, do_truncate);
    sweep_all(KIND_PRUNE, do_prune);
    sweep_all(KIND_ROTATE, do_rotate);
    TEST_END;
}
