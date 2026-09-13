/* model_fs.c - deterministic in-memory filesystem for crash and fault
 * injection. */

#include <stdlib.h>
#include <string.h>

#include "model_fs.h"

struct led89_dir
{
    char prefix[MFS_MAX_PATH];
    size_t pos;
};

static mfs_file *mfs_lookup(mfs *fs, const char *name)
{
    size_t i;

    for (i = 0u; i < fs->count; ++i)
    {
        if (strcmp(fs->files[i].name, name) == 0)
        {
            return &fs->files[i];
        }
    }
    return NULL;
}

static mfs_file *mfs_lookup_live(mfs *fs, const char *name)
{
    mfs_file *f;

    f = mfs_lookup(fs, name);
    if (f == NULL)
    {
        return NULL;
    }
    if (f->live == 0)
    {
        return NULL;
    }
    return f;
}

static void mfs_free(mfs_file *f);

static int mfs_reserve_files(mfs *fs, size_t need)
{
    mfs_file *p;
    size_t cap;

    if (need <= fs->cap)
    {
        return 1;
    }
    cap = fs->cap;
    if (cap == 0u)
    {
        cap = 16u;
    }
    while (cap < need)
    {
        cap *= 2u;
    }
    p = (mfs_file *)realloc(fs->files, cap * sizeof *p);
    if (p == NULL)
    {
        return 0;
    }
    fs->files = p;
    fs->cap = cap;
    return 1;
}

static mfs_file *mfs_alloc(mfs *fs, const char *name)
{
    mfs_file *f;
    size_t i;
    size_t n;

    n = strlen(name);
    if (n + 1u > MFS_MAX_PATH)
    {
        return NULL;
    }
    f = mfs_lookup(fs, name);
    if (f != NULL)
    {
        /* Recreating a name replaces only the live state; the previous
         * durable entry reappears if the create is not directory-synced. */
        free(f->live_data);
        f->live_data = NULL;
        f->live_size = 0u;
        f->live = 1;
        return f;
    }
    for (i = 0u; i < fs->count; ++i)
    {
        if (fs->files[i].live == 0)
        {
            if (fs->files[i].durable_live == 0)
            {
                f = &fs->files[i];
                mfs_free(f);
                memset(f, 0, sizeof *f);
                memcpy(f->name, name, n + 1u);
                f->live = 1;
                return f;
            }
        }
    }
    if (mfs_reserve_files(fs, fs->count + 1u) == 0)
    {
        return NULL;
    }
    f = &fs->files[fs->count];
    memset(f, 0, sizeof *f);
    memcpy(f->name, name, n + 1u);
    f->live = 1;
    ++fs->count;
    return f;
}

static int mfs_reserve(unsigned char **buf, size_t need)
{
    unsigned char *p;

    p = (unsigned char *)realloc(*buf, need);
    if (p == NULL)
    {
        return 0;
    }
    *buf = p;
    return 1;
}

static int mfs_write_at(unsigned char **buf, size_t *size, size_t offset,
                        const unsigned char *data, size_t len)
{
    size_t end;

    end = offset + len;
    if (mfs_reserve(buf, end) == 0)
    {
        return 0;
    }
    if (offset > *size)
    {
        memset(*buf + *size, 0, offset - *size);
    }
    if (len > 0u)
    {
        memcpy(*buf + offset, data, len);
    }
    if (end > *size)
    {
        *size = end;
    }
    return 1;
}

static void mfs_free(mfs_file *f)
{
    free(f->live_data);
    free(f->dur_data);
    f->live_data = NULL;
    f->dur_data = NULL;
    f->live_size = 0u;
    f->dur_size = 0u;
}

void mfs_init(mfs *fs)
{
    memset(fs, 0, sizeof *fs);
    fs->torn_bytes = -1;
}

void mfs_destroy(mfs *fs)
{
    size_t i;

    for (i = 0u; i < fs->count; ++i)
    {
        mfs_free(&fs->files[i]);
    }
    free(fs->files);
    fs->files = NULL;
    fs->count = 0u;
    fs->cap = 0u;
}

void mfs_crash(mfs *fs)
{
    size_t i;

    for (i = 0u; i < fs->count; ++i)
    {
        mfs_file *f;

        f = &fs->files[i];
        /* The durable state survives the crash; the live state is rebuilt
         * from it so repeated crashes without an intervening sync remain
         * stable. */
        free(f->live_data);
        f->live_data = NULL;
        f->live_size = 0u;
        if (f->dur_size > 0u)
        {
            f->live_data = (unsigned char *)malloc(f->dur_size);
            if (f->live_data != NULL)
            {
                memcpy(f->live_data, f->dur_data, f->dur_size);
                f->live_size = f->dur_size;
            }
        }
        f->live = f->durable_live;
    }
    fs->crash_armed = 0;
    fs->crash_torn = 0;
    fs->crashed = 0;
    fs->dead = 0;
    fs->torn_bytes = -1;
}

mfs_file *mfs_find(mfs *fs, const char *name)
{
    return mfs_lookup(fs, name);
}

size_t mfs_live_size(mfs *fs, const char *name)
{
    mfs_file *f;

    f = mfs_lookup_live(fs, name);
    if (f == NULL)
    {
        return 0u;
    }
    return f->live_size;
}

int mfs_exists(mfs *fs, const char *name)
{
    if (mfs_lookup_live(fs, name) == NULL)
    {
        return 0;
    }
    return 1;
}

int mfs_put(mfs *fs, const char *name, const void *data, size_t size)
{
    mfs_file *f;

    f = mfs_lookup(fs, name);
    if (f == NULL)
    {
        f = mfs_alloc(fs, name);
    }
    if (f == NULL)
    {
        return 0;
    }
    f->live = 1;
    f->durable_live = 1;
    if (mfs_write_at(&f->live_data, &f->live_size, 0u,
                     (const unsigned char *)data, size) == 0)
    {
        return 0;
    }
    free(f->dur_data);
    f->dur_data = NULL;
    f->dur_size = 0u;
    if (size > 0u)
    {
        f->dur_data = (unsigned char *)malloc(size);
        if (f->dur_data == NULL)
        {
            return 0;
        }
        memcpy(f->dur_data, data, size);
        f->dur_size = size;
    }
    return 1;
}

int mfs_insert(mfs *fs, const char *name, size_t offset, const void *data,
               size_t size)
{
    mfs_file *f;
    size_t tail;

    f = mfs_lookup_live(fs, name);
    if (f == NULL)
    {
        return 0;
    }
    if (offset > f->live_size)
    {
        return 0;
    }
    tail = f->live_size - offset;
    if (mfs_reserve(&f->live_data, f->live_size + size) == 0)
    {
        return 0;
    }
    memmove(f->live_data + offset + size, f->live_data + offset, tail);
    if (size > 0u)
    {
        memcpy(f->live_data + offset, data, size);
    }
    f->live_size += size;
    return 1;
}

int mfs_poke(mfs *fs, const char *name, size_t offset, unsigned char value)
{
    mfs_file *f;

    f = mfs_lookup_live(fs, name);
    if (f == NULL)
    {
        return 0;
    }
    if (offset >= f->live_size)
    {
        return 0;
    }
    f->live_data[offset] = value;
    return 1;
}

/* --- crash injection -------------------------------------------------- */

static int mfs_maybe_fail_at(mfs *fs, int op)
{
    if (fs->fail_at_armed == 0)
    {
        return 0;
    }
    if (fs->fail_at_op != op)
    {
        return 0;
    }
    if (fs->fail_at_skip > 0)
    {
        --fs->fail_at_skip;
        return 0;
    }
    fs->fail_at_armed = 0;
    fs->fail_at_fired = 1;
    return 1;
}

static int mfs_maybe_crash(mfs *fs, int op)
{
    if (mfs_maybe_fail_at(fs, op) != 0)
    {
        return 1;
    }
    if (fs->dead != 0)
    {
        return 1;
    }
    if (fs->crash_armed == 0)
    {
        return 0;
    }
    if (fs->crash_op != op)
    {
        return 0;
    }
    if (fs->crash_skip > 0)
    {
        --fs->crash_skip;
        return 0;
    }
    fs->crash_armed = 0;
    fs->crashed = 1;
    fs->dead = 1;
    return 1;
}

void mfs_arm_crash(mfs *fs, int op, int skip)
{
    fs->crash_op = op;
    fs->crash_skip = skip;
    fs->crash_armed = 1;
    fs->crash_torn = 0;
    fs->crashed = 0;
    fs->dead = 0;
}

void mfs_arm_crash_torn(mfs *fs, int skip)
{
    fs->crash_op = MFS_OP_PWRITE;
    fs->crash_skip = skip;
    fs->crash_armed = 1;
    fs->crash_torn = 1;
    fs->crashed = 0;
    fs->dead = 0;
}

void mfs_fail_at(mfs *fs, int op, int skip)
{
    fs->fail_at_op = op;
    fs->fail_at_skip = skip;
    fs->fail_at_armed = 1;
    fs->fail_at_fired = 0;
}

int mfs_fail_fired(const mfs *fs)
{
    return fs->fail_at_fired;
}

int mfs_crashed(const mfs *fs)
{
    return fs->crashed;
}

void mfs_clone(mfs *dst, const mfs *src)
{
    size_t i;

    mfs_destroy(dst);
    memset(dst, 0, sizeof *dst);
    dst->torn_bytes = -1;
    for (i = 0u; i < src->count; ++i)
    {
        const mfs_file *a;
        mfs_file *b;

        a = &src->files[i];
        b = mfs_alloc(dst, a->name);
        if (b == NULL)
        {
            return;
        }
        b->live = a->live;
        b->durable_live = a->durable_live;
        if (a->live_size > 0u)
        {
            if (mfs_write_at(&b->live_data, &b->live_size, 0u, a->live_data,
                             a->live_size) == 0)
            {
                return;
            }
        }
        if (a->dur_size > 0u)
        {
            b->dur_data = (unsigned char *)malloc(a->dur_size);
            if (b->dur_data == NULL)
            {
                return;
            }
            memcpy(b->dur_data, a->dur_data, a->dur_size);
            b->dur_size = a->dur_size;
        }
    }
}

static size_t mfs_torn_len(mfs *fs, size_t len)
{
    size_t n;

    n = len;
    if (fs->torn_bytes >= 0)
    {
        if ((size_t)fs->torn_bytes < len)
        {
            n = (size_t)fs->torn_bytes;
        }
        fs->torn_bytes = -1;
    }
    else if (fs->torn_bytes == MFS_TORN_HALF)
    {
        n = len / 2u;
        fs->torn_bytes = -1;
    }
    return n;
}

/* --- io vtable -------------------------------------------------------- */

static int mfs_open(void *ctx, const char *path, int flags, led89_fd *fd)
{
    mfs *fs;
    mfs_file *f;

    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_OPEN) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_open > 0)
    {
        --fs->fail_open;
        return LEDGER89_ERR_IO;
    }
    f = mfs_lookup_live(fs, path);
    if (f == NULL)
    {
        if ((flags & LED89_OPEN_CREATE) == 0)
        {
            return LEDGER89_ERR_IO;
        }
        f = mfs_alloc(fs, path);
        if (f == NULL)
        {
            return LEDGER89_ERR_NOMEM;
        }
    }
    *fd = (led89_fd)(f - fs->files) + 1;
    return LEDGER89_OK;
}

static int mfs_close(void *ctx, led89_fd fd)
{
    mfs *fs;

    fs = (mfs *)ctx;
    (void)fd;
    if (mfs_maybe_crash(fs, MFS_OP_CLOSE) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_close > 0)
    {
        --fs->fail_close;
        return LEDGER89_ERR_IO;
    }
    return LEDGER89_OK;
}

static mfs_file *mfs_by_fd(mfs *fs, led89_fd fd)
{
    if (fd <= 0)
    {
        return NULL;
    }
    if ((size_t)fd > fs->count)
    {
        return NULL;
    }
    return &fs->files[fd - 1];
}

static int mfs_pread(void *ctx, led89_fd fd, void *buf, size_t len,
                     led89_u64 offset)
{
    mfs *fs;
    mfs_file *f;

    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_PREAD) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_pread > 0)
    {
        --fs->fail_pread;
        return LEDGER89_ERR_IO;
    }
    f = mfs_by_fd(fs, fd);
    if (f == NULL)
    {
        return LEDGER89_ERR_IO;
    }
    if (f->live == 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (offset > (led89_u64)f->live_size)
    {
        return LEDGER89_ERR_IO;
    }
    if (offset + (led89_u64)len > (led89_u64)f->live_size)
    {
        return LEDGER89_ERR_IO;
    }
    if (len > 0u)
    {
        memcpy(buf, f->live_data + (size_t)offset, len);
    }
    return LEDGER89_OK;
}

static int mfs_pwrite(void *ctx, led89_fd fd, const void *buf, size_t len,
                      led89_u64 offset)
{
    mfs *fs;
    mfs_file *f;
    size_t n;
    int crash;

    fs = (mfs *)ctx;
    if (fs->fail_pwrite > 0)
    {
        --fs->fail_pwrite;
        return LEDGER89_ERR_IO;
    }
    f = mfs_by_fd(fs, fd);
    if (f == NULL)
    {
        return LEDGER89_ERR_IO;
    }
    if (f->live == 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (offset > (led89_u64)((size_t)-1))
    {
        return LEDGER89_ERR_IO;
    }
    crash = mfs_maybe_crash(fs, MFS_OP_PWRITE);
    if (crash != 0)
    {
        if (fs->crash_torn != 0)
        {
            n = len / 2u;
            if (mfs_write_at(&f->live_data, &f->live_size, (size_t)offset,
                             (const unsigned char *)buf, n) == 0)
            {
                return LEDGER89_ERR_NOMEM;
            }
        }
        return LEDGER89_ERR_IO;
    }
    n = mfs_torn_len(fs, len);
    if (mfs_write_at(&f->live_data, &f->live_size, (size_t)offset,
                     (const unsigned char *)buf, n) == 0)
    {
        return LEDGER89_ERR_NOMEM;
    }
    if (n < len)
    {
        return LEDGER89_ERR_IO;
    }
    return LEDGER89_OK;
}

static int mfs_size(void *ctx, led89_fd fd, led89_u64 *out)
{
    mfs *fs;
    mfs_file *f;

    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_SIZE) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_size > 0)
    {
        --fs->fail_size;
        return LEDGER89_ERR_IO;
    }
    f = mfs_by_fd(fs, fd);
    if (f == NULL)
    {
        return LEDGER89_ERR_IO;
    }
    *out = (led89_u64)f->live_size;
    return LEDGER89_OK;
}

static int mfs_truncate(void *ctx, led89_fd fd, led89_u64 size)
{
    mfs *fs;
    mfs_file *f;

    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_TRUNCATE) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_truncate > 0)
    {
        --fs->fail_truncate;
        return LEDGER89_ERR_IO;
    }
    f = mfs_by_fd(fs, fd);
    if (f == NULL)
    {
        return LEDGER89_ERR_IO;
    }
    if (size > (led89_u64)((size_t)-1))
    {
        return LEDGER89_ERR_IO;
    }
    if ((size_t)size > f->live_size)
    {
        if (mfs_write_at(&f->live_data, &f->live_size, f->live_size,
                         (const unsigned char *)"", 0u) == 0)
        {
            return LEDGER89_ERR_NOMEM;
        }
        if (mfs_reserve(&f->live_data, (size_t)size) == 0)
        {
            return LEDGER89_ERR_NOMEM;
        }
        memset(f->live_data + f->live_size, 0, (size_t)size - f->live_size);
    }
    f->live_size = (size_t)size;
    return LEDGER89_OK;
}

static int mfs_sync(void *ctx, led89_fd fd)
{
    mfs *fs;
    mfs_file *f;

    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_SYNC) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_sync > 0)
    {
        --fs->fail_sync;
        return LEDGER89_ERR_IO;
    }
    f = mfs_by_fd(fs, fd);
    if (f == NULL)
    {
        return LEDGER89_ERR_IO;
    }
    free(f->dur_data);
    f->dur_data = NULL;
    f->dur_size = 0u;
    if (f->live_size > 0u)
    {
        f->dur_data = (unsigned char *)malloc(f->live_size);
        if (f->dur_data == NULL)
        {
            return LEDGER89_ERR_NOMEM;
        }
        memcpy(f->dur_data, f->live_data, f->live_size);
        f->dur_size = f->live_size;
    }
    return LEDGER89_OK;
}

static int mfs_rename(void *ctx, const char *from, const char *to)
{
    mfs *fs;
    size_t ai;
    mfs_file *a;
    mfs_file *b;

    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_RENAME) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_rename > 0)
    {
        --fs->fail_rename;
        return LEDGER89_ERR_IO;
    }
    a = mfs_lookup_live(fs, from);
    if (a == NULL)
    {
        return LEDGER89_ERR_IO;
    }
    if (strcmp(from, to) == 0)
    {
        return LEDGER89_OK;
    }
    ai = (size_t)(a - fs->files);
    b = mfs_lookup(fs, to);
    if (b == NULL)
    {
        b = mfs_alloc(fs, to);
        if (b == NULL)
        {
            return LEDGER89_ERR_NOMEM;
        }
        /* mfs_alloc may have moved the file table. */
        a = &fs->files[ai];
    }
    /* Move only the live state; the target's durable state is restored if
     * the rename is not directory-synced. */
    free(b->live_data);
    b->live = 1;
    b->live_data = a->live_data;
    b->live_size = a->live_size;
    a->live_data = NULL;
    a->live_size = 0u;
    a->live = 0;
    return LEDGER89_OK;
}

static int mfs_unlink(void *ctx, const char *path)
{
    mfs *fs;
    mfs_file *f;

    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_UNLINK) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_unlink > 0)
    {
        --fs->fail_unlink;
        return LEDGER89_ERR_IO;
    }
    f = mfs_lookup_live(fs, path);
    if (f == NULL)
    {
        return LEDGER89_OK;
    }
    f->live = 0;
    return LEDGER89_OK;
}

static int mfs_mkdir(void *ctx, const char *path)
{
    mfs *fs;

    fs = (mfs *)ctx;
    (void)path;
    if (mfs_maybe_crash(fs, MFS_OP_MKDIR) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_mkdir > 0)
    {
        --fs->fail_mkdir;
        return LEDGER89_ERR_IO;
    }
    return LEDGER89_OK;
}

static int mfs_sync_dir(void *ctx, const char *path)
{
    mfs *fs;
    size_t i;
    size_t n;

    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_SYNC_DIR) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_sync_dir > 0)
    {
        --fs->fail_sync_dir;
        return LEDGER89_ERR_IO;
    }
    n = strlen(path);
    for (i = 0u; i < fs->count; ++i)
    {
        mfs_file *f;

        f = &fs->files[i];
        if (strncmp(f->name, path, n) != 0)
        {
            continue;
        }
        free(f->dur_data);
        f->dur_data = NULL;
        f->dur_size = 0u;
        f->durable_live = f->live;
        if (f->live == 0)
        {
            continue;
        }
        if (f->live_size > 0u)
        {
            f->dur_data = (unsigned char *)malloc(f->live_size);
            if (f->dur_data == NULL)
            {
                return LEDGER89_ERR_NOMEM;
            }
            memcpy(f->dur_data, f->live_data, f->live_size);
            f->dur_size = f->live_size;
        }
    }
    return LEDGER89_OK;
}

static int mfs_lock(void *ctx, const char *path, led89_fd *fd)
{
    mfs *fs;
    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_LOCK) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_lock > 0)
    {
        --fs->fail_lock;
        return LEDGER89_ERR_IO;
    }
    return mfs_open(ctx, path,
                    LED89_OPEN_READ | LED89_OPEN_WRITE | LED89_OPEN_CREATE, fd);
}

static int mfs_list_open(void *ctx, const char *path, led89_dir **out)
{
    mfs *fs;
    led89_dir *dir;
    size_t n;

    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_LIST_OPEN) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    if (fs->fail_list > 0)
    {
        --fs->fail_list;
        return LEDGER89_ERR_IO;
    }
    n = strlen(path);
    if (n + 1u > MFS_MAX_PATH)
    {
        return LEDGER89_ERR_RANGE;
    }
    dir = (led89_dir *)malloc(sizeof *dir);
    if (dir == NULL)
    {
        return LEDGER89_ERR_NOMEM;
    }
    memcpy(dir->prefix, path, n + 1u);
    dir->pos = 0u;
    *out = dir;
    return LEDGER89_OK;
}

static int mfs_list_next(void *ctx, led89_dir *dir, char *name, size_t cap,
                         int *done)
{
    mfs *fs;
    size_t plen;

    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_LIST_NEXT) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    plen = strlen(dir->prefix);
    while (dir->pos < fs->count)
    {
        mfs_file *f;
        const char *base;

        f = &fs->files[dir->pos];
        ++dir->pos;
        if (f->live == 0)
        {
            continue;
        }
        if (strncmp(f->name, dir->prefix, plen) != 0)
        {
            continue;
        }
        if (f->name[plen] != '/')
        {
            continue;
        }
        base = f->name + plen + 1u;
        if (strlen(base) + 1u > cap)
        {
            return LEDGER89_ERR_RANGE;
        }
        memcpy(name, base, strlen(base) + 1u);
        *done = 0;
        return LEDGER89_OK;
    }
    *done = 1;
    return LEDGER89_OK;
}

static int mfs_list_close(void *ctx, led89_dir *dir)
{
    mfs *fs;

    fs = (mfs *)ctx;
    if (mfs_maybe_crash(fs, MFS_OP_LIST_CLOSE) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    free(dir);
    return LEDGER89_OK;
}

void mfs_bind(led89_io *io, mfs *fs)
{
    io->ctx = fs;
    io->open = mfs_open;
    io->close = mfs_close;
    io->pread = mfs_pread;
    io->pwrite = mfs_pwrite;
    io->size = mfs_size;
    io->truncate = mfs_truncate;
    io->sync = mfs_sync;
    io->rename = mfs_rename;
    io->unlink = mfs_unlink;
    io->mkdir = mfs_mkdir;
    io->sync_dir = mfs_sync_dir;
    io->lock = mfs_lock;
    io->list_open = mfs_list_open;
    io->list_next = mfs_list_next;
    io->list_close = mfs_list_close;
}
