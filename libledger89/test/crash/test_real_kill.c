/* test_real_kill.c - real-filesystem process-kill suite.
 *
 * The parent builds a durable pre-state, forks a child that opens the
 * ledger through a process-kill I/O wrapper, and terminates the child at an
 * armed syscall boundary. The parent then reopens the ledger and checks that
 * the recovered state is legal. */

#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test.h"

#include "crash_util.h"
#include "real_io.h"
#include "tmpdir.h"

enum
{
    K_APPEND = 0,
    K_ROTATE,
    K_TRUNCATE
};

static unsigned long records_for(int kind)
{
    if (kind == K_TRUNCATE)
    {
        return 2ul;
    }
    return 0ul;
}

static int build_pre(const char *path, int kind)
{
    ledger89_config cfg;
    ledger89 *l;
    int ok;

    memset(&cfg, 0, sizeof cfg);
    cfg.path = path;
    cfg.max_segment_records = records_for(kind);
    l = NULL;
    if (ledger89_open(&l, &cfg) != LEDGER89_OK)
    {
        return 0;
    }
    ok = 1;
    if (kind == K_ROTATE)
    {
        if (cu_fill(l, 3ul) == 0)
        {
            ok = 0;
        }
        if (ok != 0 && ledger89_sync(l) != LEDGER89_OK)
        {
            ok = 0;
        }
        if (ok != 0 && ledger89_rotate(l) != LEDGER89_OK)
        {
            ok = 0;
        }
        if (ok != 0 && cu_fill_range(l, 4ul, 6ul) == 0)
        {
            ok = 0;
        }
    }
    else if (kind == K_TRUNCATE)
    {
        if (cu_fill(l, 8ul) == 0)
        {
            ok = 0;
        }
    }
    else
    {
        if (cu_fill(l, 3ul) == 0)
        {
            ok = 0;
        }
    }
    if (ok != 0 && ledger89_sync(l) != LEDGER89_OK)
    {
        ok = 0;
    }
    ledger89_close(l);
    return ok;
}

static void child_run(const char *path, int kind, int op, int skip)
{
    real_io ri;
    ledger89_config cfg;
    ledger89 *l;

    real_io_init(&ri);
    memset(&cfg, 0, sizeof cfg);
    cfg.path = path;
    cfg.max_segment_records = records_for(kind);
    l = NULL;
    if (led89_open_io(&l, &cfg, real_io_api(&ri)) != LEDGER89_OK)
    {
        _exit(1);
    }
    real_io_arm(&ri, op, skip);
    if (kind == K_APPEND)
    {
        (void)cu_append(l, 4ul);
        (void)ledger89_sync(l);
    }
    else if (kind == K_ROTATE)
    {
        (void)ledger89_rotate(l);
    }
    else
    {
        (void)ledger89_truncate_after(l, 3ul);
    }
    ledger89_close(l);
    _exit(0);
}

static void run_case(int kind, int op, int skip)
{
    char path[64];
    pid_t pid;
    int status;
    ledger89_config cfg;
    ledger89 *l;
    ledger89_index first;
    ledger89_index last;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(build_pre(path, kind) != 0);

    pid = fork();
    CHECK(pid >= 0);
    if (pid == 0)
    {
        child_run(path, kind, op, skip);
        _exit(1);
    }
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) != 0);
    if (WIFEXITED(status) != 0)
    {
        CHECK(WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == 99);
    }

    memset(&cfg, 0, sizeof cfg);
    cfg.path = path;
    cfg.max_segment_records = records_for(kind);
    l = NULL;
    CHECK_EQ(ledger89_open(&l, &cfg), LEDGER89_OK);
    if (l == NULL)
    {
        return;
    }
    first = ledger89_first_index(l);
    last = ledger89_last_index(l);
    if (kind == K_TRUNCATE)
    {
        CHECK_EQ(first, 1ul);
        CHECK(last >= 3ul);
        CHECK(last <= 8ul);
        CHECK(cu_check_range(l, 1ul, last) != 0);
    }
    else if (kind == K_ROTATE)
    {
        CHECK_EQ(last, 6ul);
        CHECK(cu_check_range(l, 1ul, 6ul) != 0);
    }
    else
    {
        CHECK(last >= 3ul);
        CHECK(last <= 4ul);
        CHECK(cu_check_range(l, 1ul, last) != 0);
    }
    ledger89_close(l);
}

int main(void)
{
    run_case(K_APPEND, MFS_OP_PWRITE, 1);
    run_case(K_APPEND, MFS_OP_PWRITE, 3);
    run_case(K_APPEND, MFS_OP_SYNC, 0);
    run_case(K_ROTATE, MFS_OP_PWRITE, 0);
    run_case(K_ROTATE, MFS_OP_SYNC_DIR, 0);
    run_case(K_TRUNCATE, MFS_OP_UNLINK, 0);
    run_case(K_TRUNCATE, MFS_OP_UNLINK, 1);
    run_case(K_TRUNCATE, MFS_OP_RENAME, 0);

    TEST_END;
}
