/* ledger89_file.c - POSIX implementation of the internal I/O vtable.
 *
 * Every callback returns LEDGER89_OK or a negative status. EINTR is retried
 * where retrying is safe; close() is never retried. Short pread/pwrite calls
 * are looped so the caller sees all-or-error semantics. */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ledger89_internal.h"

struct led89_dir
{
    DIR *dirp;
};

static int led89_off_ok(led89_u64 v)
{
    led89_u64 max;

    max = ~((led89_u64)0) >> 1;
    if (v > max)
    {
        return 0;
    }
    return 1;
}

static int led89_dot_name(const char *name)
{
    if (strcmp(name, ".") == 0)
    {
        return 1;
    }
    if (strcmp(name, "..") == 0)
    {
        return 1;
    }
    return 0;
}

static int led89_add_flag(int value, int flag)
{
    return value | flag;
}

static int led89_open_oflags(int flags)
{
    int oflags;

    oflags = O_RDONLY;
    if ((flags & LED89_OPEN_WRITE) != 0)
    {
        oflags = O_RDWR;
    }
    if ((flags & LED89_OPEN_CREATE) != 0)
    {
        oflags = led89_add_flag(oflags, O_CREAT);
    }
    return oflags;
}

static int led89_posix_open(void *ctx, const char *path, int flags,
                            led89_fd *fd)
{
    int oflags;
    int rc;

    (void)ctx;
    oflags = led89_open_oflags(flags);
    for (;;)
    {
        rc = open(path, oflags, 0666);
        if (rc >= 0)
        {
            break;
        }
        if (errno != EINTR)
        {
            return LEDGER89_ERR_IO;
        }
    }
    *fd = rc;
    return LEDGER89_OK;
}

static int led89_posix_close(void *ctx, led89_fd fd)
{
    int rc;

    (void)ctx;
    rc = close(fd);
    if (rc != 0)
    {
        return LEDGER89_ERR_IO;
    }
    return LEDGER89_OK;
}

static int led89_pread_once(led89_fd fd, unsigned char *p, size_t len,
                            led89_u64 offset, size_t *done)
{
    led89_u64 at;
    ssize_t n;
    int rc;

    at = offset + (led89_u64)*done;
    rc = led89_off_ok(at);
    if (rc == 0)
    {
        return LEDGER89_ERR_RANGE;
    }
    n = pread(fd, p + *done, len - *done, (off_t)at);
    if (n < 0)
    {
        if (errno == EINTR)
        {
            return LEDGER89_OK;
        }
        return LEDGER89_ERR_IO;
    }
    if (n == 0)
    {
        return LEDGER89_ERR_IO;
    }
    *done += (size_t)n;
    return LEDGER89_OK;
}

static int led89_posix_pread(void *ctx, led89_fd fd, void *buf, size_t len,
                             led89_u64 offset)
{
    unsigned char *p;
    size_t done;
    int rc;

    (void)ctx;
    p = (unsigned char *)buf;
    done = 0u;
    while (done < len)
    {
        rc = led89_pread_once(fd, p, len, offset, &done);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    return LEDGER89_OK;
}

static int led89_pwrite_once(led89_fd fd, const unsigned char *p, size_t len,
                             led89_u64 offset, size_t *done)
{
    led89_u64 at;
    ssize_t n;
    int rc;

    at = offset + (led89_u64)*done;
    rc = led89_off_ok(at);
    if (rc == 0)
    {
        return LEDGER89_ERR_RANGE;
    }
    n = pwrite(fd, p + *done, len - *done, (off_t)at);
    if (n < 0)
    {
        if (errno == EINTR)
        {
            return LEDGER89_OK;
        }
        return LEDGER89_ERR_IO;
    }
    if (n == 0)
    {
        return LEDGER89_ERR_IO;
    }
    *done += (size_t)n;
    return LEDGER89_OK;
}

static int led89_posix_pwrite(void *ctx, led89_fd fd, const void *buf,
                              size_t len, led89_u64 offset)
{
    const unsigned char *p;
    size_t done;
    int rc;

    (void)ctx;
    p = (const unsigned char *)buf;
    done = 0u;
    while (done < len)
    {
        rc = led89_pwrite_once(fd, p, len, offset, &done);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    return LEDGER89_OK;
}

static int led89_posix_size(void *ctx, led89_fd fd, led89_u64 *out)
{
    struct stat st;
    int rc;

    (void)ctx;
    for (;;)
    {
        rc = fstat(fd, &st);
        if (rc == 0)
        {
            break;
        }
        if (errno != EINTR)
        {
            return LEDGER89_ERR_IO;
        }
    }
    if (st.st_size < 0)
    {
        return LEDGER89_ERR_IO;
    }
    *out = (led89_u64)st.st_size;
    return LEDGER89_OK;
}

static int led89_posix_truncate(void *ctx, led89_fd fd, led89_u64 size)
{
    int rc;

    (void)ctx;
    if (led89_off_ok(size) == 0)
    {
        return LEDGER89_ERR_RANGE;
    }
    for (;;)
    {
        rc = ftruncate(fd, (off_t)size);
        if (rc == 0)
        {
            break;
        }
        if (errno != EINTR)
        {
            return LEDGER89_ERR_IO;
        }
    }
    return LEDGER89_OK;
}

static int led89_posix_sync(void *ctx, led89_fd fd)
{
    int rc;

    (void)ctx;
    for (;;)
    {
        rc = fdatasync(fd);
        if (rc == 0)
        {
            break;
        }
        if (errno != EINTR)
        {
            return LEDGER89_ERR_IO;
        }
    }
    return LEDGER89_OK;
}

static int led89_posix_fsync(int fd)
{
    int rc;

    for (;;)
    {
        rc = fsync(fd);
        if (rc == 0)
        {
            break;
        }
        if (errno != EINTR)
        {
            return LEDGER89_ERR_IO;
        }
    }
    return LEDGER89_OK;
}

static int led89_posix_rename(void *ctx, const char *from, const char *to)
{
    int rc;

    (void)ctx;
    for (;;)
    {
        rc = rename(from, to);
        if (rc == 0)
        {
            return LEDGER89_OK;
        }
        if (errno != EINTR)
        {
            return LEDGER89_ERR_IO;
        }
    }
}

static int led89_posix_unlink(void *ctx, const char *path)
{
    int rc;

    (void)ctx;
    for (;;)
    {
        rc = unlink(path);
        if (rc == 0)
        {
            return LEDGER89_OK;
        }
        if (errno == ENOENT)
        {
            return LEDGER89_OK;
        }
        if (errno != EINTR)
        {
            return LEDGER89_ERR_IO;
        }
    }
}

static int led89_posix_mkdir(void *ctx, const char *path)
{
    int rc;

    (void)ctx;
    for (;;)
    {
        rc = mkdir(path, 0777);
        if (rc == 0)
        {
            return LEDGER89_OK;
        }
        if (errno == EEXIST)
        {
            return LEDGER89_OK;
        }
        if (errno != EINTR)
        {
            return LEDGER89_ERR_IO;
        }
    }
}

static int led89_posix_sync_dir(void *ctx, const char *path)
{
    int fd;
    int rc;

    (void)ctx;
    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        return LEDGER89_ERR_IO;
    }
    rc = led89_posix_fsync(fd);
    (void)close(fd);
    return rc;
}

static int led89_posix_lock_fail(int fd)
{
    int err;

    err = errno;
    (void)close(fd);
    if (err == EWOULDBLOCK)
    {
        return LEDGER89_ERR_BUSY;
    }
    return LEDGER89_ERR_IO;
}

static int led89_posix_lock(void *ctx, const char *path, led89_fd *fd)
{
    int f;
    int rc;

    (void)ctx;
    f = open(path, O_RDWR | O_CREAT, 0666);
    if (f < 0)
    {
        return LEDGER89_ERR_IO;
    }
    rc = flock(f, LOCK_EX | LOCK_NB);
    if (rc != 0)
    {
        rc = led89_posix_lock_fail(f);
        return rc;
    }
    *fd = f;
    return LEDGER89_OK;
}

static int led89_posix_list_open(void *ctx, const char *path, led89_dir **out)
{
    DIR *d;
    led89_dir *dir;

    (void)ctx;
    d = opendir(path);
    if (d == NULL)
    {
        return LEDGER89_ERR_IO;
    }
    dir = (led89_dir *)malloc(sizeof *dir);
    if (dir == NULL)
    {
        closedir(d);
        return LEDGER89_ERR_NOMEM;
    }
    dir->dirp = d;
    *out = dir;
    return LEDGER89_OK;
}

static int led89_posix_next_entry(DIR *d, struct dirent **out)
{
    for (;;)
    {
        *out = readdir(d);
        if (*out == NULL)
        {
            return LEDGER89_OK;
        }
        if (led89_dot_name((*out)->d_name) == 0)
        {
            return LEDGER89_OK;
        }
    }
}

static int led89_posix_take_name(const char *src, char *name, size_t cap)
{
    size_t n;

    n = strlen(src);
    if (n + 1u > cap)
    {
        return LEDGER89_ERR_RANGE;
    }
    memcpy(name, src, n + 1u);
    return LEDGER89_OK;
}

static int led89_posix_list_next(void *ctx, led89_dir *dir, char *name,
                                 size_t cap, int *done)
{
    struct dirent *ent;
    int rc;

    (void)ctx;
    rc = led89_posix_next_entry(dir->dirp, &ent);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (ent == NULL)
    {
        *done = 1;
        return LEDGER89_OK;
    }
    rc = led89_posix_take_name(ent->d_name, name, cap);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *done = 0;
    return LEDGER89_OK;
}

static int led89_posix_list_close(void *ctx, led89_dir *dir)
{
    int rc;

    (void)ctx;
    rc = closedir(dir->dirp);
    free(dir);
    if (rc != 0)
    {
        return LEDGER89_ERR_IO;
    }
    return LEDGER89_OK;
}

static const led89_io led89_posix_impl = {NULL,
                                          led89_posix_open,
                                          led89_posix_close,
                                          led89_posix_pread,
                                          led89_posix_pwrite,
                                          led89_posix_size,
                                          led89_posix_truncate,
                                          led89_posix_sync,
                                          led89_posix_rename,
                                          led89_posix_unlink,
                                          led89_posix_mkdir,
                                          led89_posix_sync_dir,
                                          led89_posix_lock,
                                          led89_posix_list_open,
                                          led89_posix_list_next,
                                          led89_posix_list_close};

const led89_io *led89_io_posix(void)
{
    return &led89_posix_impl;
}
