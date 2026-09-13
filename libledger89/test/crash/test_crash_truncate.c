/* test_crash_truncate.c - truncation and discard crash matrices over the
 * model filesystem.
 *
 * Sweeps every call of each persistence primitive during one truncation or
 * discard and asserts that recovery exposes a contiguous prefix (truncate)
 * or suffix (discard). */

#include <string.h>

#include "test.h"

#include "crash_util.h"

enum
{
    OP_TRUNCATE = 0,
    OP_DISCARD,
    OP_DISCARD_ALL
};

static int fired_count;

static int run_case(const mfs *base, int op, int crash_op, int torn, int skip)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_index first;
    ledger89_index last;
    ledger89_index next;
    int fired;

    mfs_init(&fs);
    mfs_clone(&fs, base);
    CHECK_EQ(cu_open(&fs, &io, &l, 2ul), LEDGER89_OK);
    if (torn != 0)
    {
        mfs_arm_crash_torn(&fs, skip);
    }
    else
    {
        mfs_arm_crash(&fs, crash_op, skip);
    }
    if (op == OP_TRUNCATE)
    {
        (void)ledger89_truncate_after(l, 3ul);
    }
    else if (op == OP_DISCARD)
    {
        (void)ledger89_discard_before(l, 4ul);
    }
    else
    {
        (void)ledger89_discard_before(l, 99ul);
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
    CHECK_EQ(cu_open(&fs, &io, &l, 2ul), LEDGER89_OK);
    first = ledger89_first_index(l);
    last = ledger89_last_index(l);
    if (op == OP_TRUNCATE)
    {
        CHECK_EQ(first, 1ul);
        CHECK(last >= 3ul);
        CHECK(last <= 8ul);
        CHECK(cu_check_range(l, 1ul, last) != 0);
        next = last + 1ul;
    }
    else if (op == OP_DISCARD)
    {
        CHECK(first >= 1ul);
        CHECK(first <= 4ul);
        CHECK_EQ(last, 8ul);
        CHECK(cu_check_range(l, first, last) != 0);
        next = 9ul;
    }
    else
    {
        CHECK(first >= 1ul);
        CHECK(first <= 9ul);
        CHECK_EQ(last, 8ul);
        CHECK(cu_check_range(l, first, last) != 0);
        next = 9ul;
    }
    CHECK_EQ(cu_append(l, next), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);
    mfs_destroy(&fs);
    return 1;
}

static void run_sweep(const mfs *base, int op, int crash_op, int torn)
{
    int skip;

    for (skip = 0;; ++skip)
    {
        if (run_case(base, op, crash_op, torn, skip) == 0)
        {
            break;
        }
    }
}

static void run_ops(const mfs *base, int op)
{
    run_sweep(base, op, MFS_OP_UNLINK, 0);
    run_sweep(base, op, MFS_OP_PWRITE, 0);
    run_sweep(base, op, MFS_OP_PWRITE, 1);
    run_sweep(base, op, MFS_OP_RENAME, 0);
    run_sweep(base, op, MFS_OP_SYNC, 0);
    run_sweep(base, op, MFS_OP_SYNC_DIR, 0);
    run_sweep(base, op, MFS_OP_TRUNCATE, 0);
}

int main(void)
{
    mfs base;
    led89_io io;
    ledger89 *l;

    /* Pre-state: records 1..8 durable, sealed 1..2, 3..4, 5..6, active 7..8. */
    mfs_init(&base);
    CHECK_EQ(cu_open(&base, &io, &l, 2ul), LEDGER89_OK);
    CHECK(cu_fill(l, 8ul) != 0);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);

    run_ops(&base, OP_TRUNCATE);
    run_ops(&base, OP_DISCARD);
    run_ops(&base, OP_DISCARD_ALL);
    CHECK(fired_count >= 24);

    mfs_destroy(&base);

    TEST_END;
}
