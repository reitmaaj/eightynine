/* test_fault_matrix.c - I/O failure matrix over the model filesystem.
 *
 * Mirrors the crash sweeps, but injects a single failing call instead of a
 * power loss: the process survives, the handle faults, and reopening sees
 * the live (partially modified) state. Recovery must expose a legal
 * contiguous state, and the ledger must remain usable afterwards. */

#include <string.h>

#include "test.h"

#include "crash_util.h"

enum
{
    OP_APPEND = 0,
    OP_ROTATE,
    OP_TRUNCATE,
    OP_DISCARD,
    OP_DISCARD_ALL
};

static int fired_count;

static unsigned long records_for(int op)
{
    if (op == OP_APPEND)
    {
        return 0ul;
    }
    if (op == OP_ROTATE)
    {
        return 0ul;
    }
    return 2ul;
}

static int run_case(const mfs *base, int op, int fail_op, int skip)
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
    CHECK_EQ(cu_open(&fs, &io, &l, records_for(op)), LEDGER89_OK);
    mfs_fail_at(&fs, fail_op, skip);
    if (op == OP_APPEND)
    {
        (void)cu_append(l, 4ul);
    }
    else if (op == OP_ROTATE)
    {
        (void)ledger89_rotate(l);
    }
    else if (op == OP_TRUNCATE)
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
    fired = mfs_fail_fired(&fs);
    if (fired == 0)
    {
        ledger89_close(l);
        mfs_destroy(&fs);
        return 0;
    }
    ++fired_count;
    ledger89_close(l);
    CHECK_EQ(cu_open(&fs, &io, &l, records_for(op)), LEDGER89_OK);
    if (l == NULL)
    {
        mfs_destroy(&fs);
        return 1;
    }
    first = ledger89_first_index(l);
    last = ledger89_last_index(l);
    if (op == OP_APPEND)
    {
        CHECK(last >= 3ul);
        CHECK(last <= 4ul);
        CHECK(cu_check_range(l, 1ul, last) != 0);
        next = last + 1ul;
    }
    else if (op == OP_ROTATE)
    {
        CHECK_EQ(last, 6ul);
        CHECK(cu_check_range(l, 1ul, 6ul) != 0);
        next = 7ul;
    }
    else if (op == OP_TRUNCATE)
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

static void run_sweep(const mfs *base, int op, int fail_op)
{
    int skip;

    for (skip = 0;; ++skip)
    {
        if (run_case(base, op, fail_op, skip) == 0)
        {
            break;
        }
    }
}

static void run_ops(const mfs *base, int op)
{
    run_sweep(base, op, MFS_OP_PWRITE);
    run_sweep(base, op, MFS_OP_SYNC);
    run_sweep(base, op, MFS_OP_RENAME);
    run_sweep(base, op, MFS_OP_UNLINK);
    run_sweep(base, op, MFS_OP_SYNC_DIR);
    run_sweep(base, op, MFS_OP_TRUNCATE);
    run_sweep(base, op, MFS_OP_PREAD);
    run_sweep(base, op, MFS_OP_SIZE);
}

static int build_base(mfs *base, int op)
{
    led89_io io;
    ledger89 *l;

    mfs_init(base);
    CHECK_EQ(cu_open(base, &io, &l, records_for(op)), LEDGER89_OK);
    if (l == NULL)
    {
        return 0;
    }
    if (op == OP_ROTATE)
    {
        CHECK(cu_fill(l, 3ul) != 0);
        CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
        CHECK_EQ(ledger89_rotate(l), LEDGER89_OK);
        CHECK(cu_fill_range(l, 4ul, 6ul) != 0);
    }
    else if (op == OP_APPEND)
    {
        CHECK(cu_fill(l, 3ul) != 0);
    }
    else
    {
        CHECK(cu_fill(l, 8ul) != 0);
    }
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);
    return 1;
}

int main(void)
{
    mfs base;

    CHECK(build_base(&base, OP_APPEND) != 0);
    run_ops(&base, OP_APPEND);
    mfs_destroy(&base);

    CHECK(build_base(&base, OP_ROTATE) != 0);
    run_ops(&base, OP_ROTATE);
    mfs_destroy(&base);

    CHECK(build_base(&base, OP_TRUNCATE) != 0);
    run_ops(&base, OP_TRUNCATE);
    mfs_destroy(&base);

    CHECK(build_base(&base, OP_DISCARD) != 0);
    run_ops(&base, OP_DISCARD);
    mfs_destroy(&base);

    CHECK(build_base(&base, OP_DISCARD_ALL) != 0);
    run_ops(&base, OP_DISCARD_ALL);
    mfs_destroy(&base);

    CHECK(fired_count >= 30);

    TEST_END;
}
