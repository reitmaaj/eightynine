/* test_real_kill.c - real-filesystem process kills at armed syscalls. */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "real_io.h"
#include "test.h"
#include "tmpdir.h"

static int open_plain(const char *path, ledger89 **l)
{
    *l = NULL;
    return ledger89_open(l, path, LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE);
}

static int make_ledger(const char *path, unsigned long count, int rotate_after)
{
    ledger89 *l;
    unsigned long i;

    if (open_plain(path, &l) != LEDGER89_OK)
    {
        return 0;
    }
    for (i = 1ul; i <= count; ++i)
    {
        unsigned char v;
        ledger89_slice s;

        v = (unsigned char)('a' + (int)(i % 26ul));
        s.data = &v;
        s.size = 1u;
        if (ledger89_appendv(l, &s, 1u, NULL) != LEDGER89_OK)
        {
            ledger89_close(l);
            return 0;
        }
        if (rotate_after != 0 && i == (count / 2ul))
        {
            if (ledger89_sync(l, NULL) != LEDGER89_OK ||
                ledger89_rotate(l) != LEDGER89_OK)
            {
                ledger89_close(l);
                return 0;
            }
        }
    }
    if (ledger89_sync(l, NULL) != LEDGER89_OK)
    {
        ledger89_close(l);
        return 0;
    }
    ledger89_close(l);
    return 1;
}

enum kill_kind
{
    KILL_APPEND = 0,
    KILL_TRUNCATE = 1,
    KILL_PRUNE = 2
};

static void child_run(const char *path, int op, int skip, int kind)
{
    real_io r;
    ledger89 *l;
    unsigned char v;
    ledger89_slice s;

    real_io_init(&r);
    real_io_arm(&r, op, skip);
    l = NULL;
    if (led89_open_io(&l, path, LEDGER89_OPEN_RDWR, real_io_api(&r)) !=
        LEDGER89_OK)
    {
        _exit(0);
    }
    v = 'z';
    s.data = &v;
    s.size = 1u;
    if (kind == KILL_APPEND)
    {
        (void)ledger89_appendv(l, &s, 1u, NULL);
        (void)ledger89_sync(l, NULL);
    }
    else if (kind == KILL_TRUNCATE)
    {
        (void)ledger89_truncate_from(l, test_u64(2));
    }
    else
    {
        ledger89_index actual;

        (void)ledger89_prune_before(l, test_u64(4), &actual);
    }
    _exit(0);
}

static void check_append_state(ledger89 *l)
{
    ledger89_state st;
    unsigned long end;

    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    end = st.end.lo;
    CHECK(end >= 4ul && end <= 5ul);
    CHECK_U64(st.stable_end, st.end);
    if (end == 5ul)
    {
        unsigned char buf[2];
        size_t size;

        CHECK_EQ(ledger89_read(l, test_u64(4), buf, sizeof buf, &size),
                 LEDGER89_OK);
        CHECK_EQ(size, 1u);
        CHECK_EQ(buf[0], (unsigned char)'z');
    }
}

static void check_truncate_state(ledger89 *l)
{
    ledger89_state st;
    unsigned char buf[2];
    size_t size;

    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.stable_end, st.end);
    if (st.revision.lo == 0u)
    {
        CHECK_U64(st.end, test_u64(7));
    }
    else
    {
        CHECK_EQ(st.revision.lo, 1u);
        CHECK_U64(st.end, test_u64(2));
        CHECK_EQ(ledger89_read(l, test_u64(1), buf, sizeof buf, &size),
                 LEDGER89_OK);
    }
}

static void check_prune_state(ledger89 *l)
{
    ledger89_state st;
    unsigned char buf[2];
    size_t size;

    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.end, test_u64(7));
    CHECK_EQ(st.revision.lo, 0u);
    CHECK(st.first.lo == 1u || st.first.lo == 4u);
    CHECK_EQ(ledger89_read(l, st.first, buf, sizeof buf, &size), LEDGER89_OK);
}

static void sweep(int op, int kind, int pre_rotate)
{
    int skip;

    for (skip = 0; skip < 200; ++skip)
    {
        char path[64];
        ledger89 *l;
        pid_t pid;
        int status;

        CHECK(tmpdir_create(path, sizeof path) == 0);
        CHECK(make_ledger(path, pre_rotate ? 6ul : 3ul, pre_rotate) != 0);
        pid = fork();
        if (pid == 0)
        {
            child_run(path, op, skip, kind);
            _exit(0);
        }
        CHECK(pid > 0);
        if (pid <= 0)
        {
            return;
        }
        CHECK(waitpid(pid, &status, 0) == pid);
        if (!(WIFEXITED(status) && WEXITSTATUS(status) == 99))
        {
            /* The armed syscall never fired during this iteration. */
            return;
        }
        CHECK_EQ(open_plain(path, &l), LEDGER89_OK);
        if (l == NULL)
        {
            return;
        }
        if (kind == KILL_APPEND)
        {
            check_append_state(l);
        }
        else if (kind == KILL_TRUNCATE)
        {
            check_truncate_state(l);
        }
        else
        {
            check_prune_state(l);
        }
        ledger89_close(l);
    }
    CHECK(0);
}

int main(void)
{
    sweep(MFS_OP_PWRITE, KILL_APPEND, 0);
    sweep(MFS_OP_SYNC, KILL_APPEND, 0);
    sweep(MFS_OP_PWRITE, KILL_TRUNCATE, 1);
    sweep(MFS_OP_RENAME, KILL_TRUNCATE, 1);
    sweep(MFS_OP_UNLINK, KILL_TRUNCATE, 1);
    sweep(MFS_OP_RENAME, KILL_PRUNE, 1);
    sweep(MFS_OP_UNLINK, KILL_PRUNE, 1);
    TEST_END;
}
