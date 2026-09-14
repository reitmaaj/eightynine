/* real_io.c - process-kill wrapper around the POSIX I/O vtable. */

#include <stdlib.h>
#include <unistd.h>

#include "real_io.h"

static int real_maybe_crash(real_io *r, int op)
{
    if (r->crash_armed == 0)
    {
        return 0;
    }
    if (r->crash_op != op)
    {
        return 0;
    }
    if (r->crash_skip > 0)
    {
        --r->crash_skip;
        return 0;
    }
    r->crash_armed = 0;
    _exit(99);
    return 1;
}

static int real_open(void *ctx, const char *path, int flags, led89_fd *fd)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_OPEN);
    return led89_io_posix()->open(NULL, path, flags, fd);
}

static int real_close(void *ctx, led89_fd fd)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_CLOSE);
    return led89_io_posix()->close(NULL, fd);
}

static int real_pread(void *ctx, led89_fd fd, void *buf, size_t len,
                      led89_u64 offset)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_PREAD);
    return led89_io_posix()->pread(NULL, fd, buf, len, offset);
}

static int real_pwrite(void *ctx, led89_fd fd, const void *buf, size_t len,
                       led89_u64 offset)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_PWRITE);
    return led89_io_posix()->pwrite(NULL, fd, buf, len, offset);
}

static int real_size(void *ctx, led89_fd fd, led89_u64 *out)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_SIZE);
    return led89_io_posix()->size(NULL, fd, out);
}

static int real_truncate(void *ctx, led89_fd fd, led89_u64 size)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_TRUNCATE);
    return led89_io_posix()->truncate(NULL, fd, size);
}

static int real_sync(void *ctx, led89_fd fd)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_SYNC);
    return led89_io_posix()->sync(NULL, fd);
}

static int real_rename(void *ctx, const char *from, const char *to)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_RENAME);
    return led89_io_posix()->rename(NULL, from, to);
}

static int real_unlink(void *ctx, const char *path)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_UNLINK);
    return led89_io_posix()->unlink(NULL, path);
}

static int real_mkdir(void *ctx, const char *path)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_MKDIR);
    return led89_io_posix()->mkdir(NULL, path);
}

static int real_sync_dir(void *ctx, const char *path)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_SYNC_DIR);
    return led89_io_posix()->sync_dir(NULL, path);
}

static int real_lock(void *ctx, const char *path, int exclusive, int create,
                     led89_fd *fd)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_LOCK);
    return led89_io_posix()->lock(NULL, path, exclusive, create, fd);
}

static int real_entropy(void *ctx, unsigned char *out, size_t len)
{
    real_io *r;

    r = (real_io *)ctx;
    real_maybe_crash(r, MFS_OP_ENTROPY);
    if (r->fixed_entropy != 0)
    {
        size_t i;

        for (i = 0u; i < len; ++i)
        {
            out[i] = r->entropy[i % 16u];
        }
        return LEDGER89_OK;
    }
    return led89_io_posix()->entropy(NULL, out, len);
}

static int real_list_open(void *ctx, const char *path, led89_dir **out)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_LIST_OPEN);
    return led89_io_posix()->list_open(NULL, path, out);
}

static int real_list_next(void *ctx, led89_dir *dir, char *name, size_t cap,
                          int *done)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_LIST_NEXT);
    return led89_io_posix()->list_next(NULL, dir, name, cap, done);
}

static int real_list_close(void *ctx, led89_dir *dir)
{
    real_maybe_crash((real_io *)ctx, MFS_OP_LIST_CLOSE);
    return led89_io_posix()->list_close(NULL, dir);
}

void real_io_init(real_io *r)
{
    r->crash_op = 0;
    r->crash_skip = 0;
    r->crash_armed = 0;
    r->fixed_entropy = 0;
    r->api.ctx = r;
    r->api.open = real_open;
    r->api.close = real_close;
    r->api.pread = real_pread;
    r->api.pwrite = real_pwrite;
    r->api.size = real_size;
    r->api.truncate = real_truncate;
    r->api.sync = real_sync;
    r->api.rename = real_rename;
    r->api.unlink = real_unlink;
    r->api.mkdir = real_mkdir;
    r->api.sync_dir = real_sync_dir;
    r->api.lock = real_lock;
    r->api.list_open = real_list_open;
    r->api.list_next = real_list_next;
    r->api.list_close = real_list_close;
    r->api.entropy = real_entropy;
}

void real_io_fix_entropy(real_io *r, const unsigned char bytes[16])
{
    size_t i;

    r->fixed_entropy = 1;
    for (i = 0u; i < 16u; ++i)
    {
        r->entropy[i] = bytes[i];
    }
}

void real_io_arm(real_io *r, int op, int skip)
{
    r->crash_op = op;
    r->crash_skip = skip;
    r->crash_armed = 1;
}

const led89_io *real_io_api(real_io *r)
{
    return &r->api;
}
