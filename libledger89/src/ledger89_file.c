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

static void led89_close_fd(int fd);

static int led89_open_retry(const char *path, int oflags)
{
    int fd;

    for (;;)
    {
        fd = open(path, oflags, 0666);
        if (fd >= 0)
        {
            return fd;
        }
        if (errno != EINTR)
        {
            return fd;
        }
    }
}

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
    rc = led89_open_retry(path, oflags);
    if (rc < 0)
    {
        if (errno == ENOENT)
        {
            return LEDGER89_ENOENT;
        }
        return LEDGER89_EIO;
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
        return LEDGER89_EIO;
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
        return LEDGER89_ERANGE;
    }
    n = pread(fd, p + *done, len - *done, (off_t)at);
    if (n < 0)
    {
        if (errno == EINTR)
        {
            return LEDGER89_OK;
        }
        return LEDGER89_EIO;
    }
    if (n == 0)
    {
        return LEDGER89_EIO;
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
        return LEDGER89_ERANGE;
    }
    n = pwrite(fd, p + *done, len - *done, (off_t)at);
    if (n < 0)
    {
        if (errno == EINTR)
        {
            return LEDGER89_OK;
        }
        return LEDGER89_EIO;
    }
    if (n == 0)
    {
        return LEDGER89_EIO;
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
            return LEDGER89_EIO;
        }
    }
    if (st.st_size < 0)
    {
        return LEDGER89_EIO;
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
        return LEDGER89_ERANGE;
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
            return LEDGER89_EIO;
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
            return LEDGER89_EIO;
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
            return LEDGER89_EIO;
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
            return LEDGER89_EIO;
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
            return LEDGER89_EIO;
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
            return LEDGER89_EIO;
        }
    }
}

static int led89_posix_sync_dir(void *ctx, const char *path)
{
    int fd;
    int rc;

    (void)ctx;
    fd = led89_open_retry(path, O_RDONLY);
    if (fd < 0)
    {
        return LEDGER89_EIO;
    }
    rc = led89_posix_fsync(fd);
    led89_close_fd(fd);
    return rc;
}

static int led89_posix_lock_fail(int fd)
{
    int err;

    err = errno;
    led89_close_fd(fd);
    if (err == EWOULDBLOCK)
    {
        return LEDGER89_EBUSY;
    }
    return LEDGER89_EIO;
}

static int led89_lock_oflags(void)
{
    return O_RDWR | O_CREAT;
}

static unsigned char *led89_buf_at(unsigned char *buf, size_t off)
{
    return buf + off;
}

static int led89_posix_lock(void *ctx, const char *path, int exclusive,
                            int create, led89_fd *fd)
{
    int f;
    int rc;
    int mode;
    int oflags;

    (void)ctx;
    mode = LOCK_SH;
    if (exclusive != 0)
    {
        mode = LOCK_EX;
    }
    oflags = O_RDONLY;
    if (create != 0)
    {
        oflags = led89_lock_oflags();
    }
    f = led89_open_retry(path, oflags);
    if (f < 0)
    {
        if (errno == ENOENT)
        {
            return LEDGER89_ENOENT;
        }
        return LEDGER89_EIO;
    }
    for (;;)
    {
        rc = flock(f, mode | LOCK_NB);
        if (rc == 0)
        {
            break;
        }
        if (errno != EINTR)
        {
            rc = led89_posix_lock_fail(f);
            return rc;
        }
    }
    *fd = f;
    return LEDGER89_OK;
}

static size_t led89_size_add(size_t a, size_t b)
{
    return a + b;
}

static size_t led89_size_left(size_t total, size_t done)
{
    return total - done;
}

static size_t led89_ssize_to_size(ssize_t n)
{
    return (size_t)n;
}

static void led89_close_fd(int fd)
{
    (void)close(fd);
}

static int led89_posix_entropy(void *ctx, unsigned char *out, size_t len)
{
    int fd;
    size_t done;
    ssize_t n;

    (void)ctx;
    fd = led89_open_retry("/dev/urandom", O_RDONLY);
    if (fd < 0)
    {
        return LEDGER89_EIO;
    }
    done = 0u;
    while (done < len)
    {
        n = read(fd, led89_buf_at(out, done), led89_size_left(len, done));
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            led89_close_fd(fd);
            return LEDGER89_EIO;
        }
        if (n == 0)
        {
            led89_close_fd(fd);
            return LEDGER89_EIO;
        }
        done = led89_size_add(done, led89_ssize_to_size(n));
    }
    led89_close_fd(fd);
    return LEDGER89_OK;
}

static DIR *led89_opendir_retry(const char *path)
{
    DIR *d;

    for (;;)
    {
        d = opendir(path);
        if (d != NULL)
        {
            return d;
        }
        if (errno != EINTR)
        {
            return d;
        }
    }
}

static int led89_posix_list_open(void *ctx, const char *path, led89_dir **out)
{
    DIR *d;
    led89_dir *dir;

    (void)ctx;
    d = led89_opendir_retry(path);
    if (d == NULL)
    {
        return LEDGER89_EIO;
    }
    dir = (led89_dir *)malloc(sizeof *dir);
    if (dir == NULL)
    {
        closedir(d);
        return LEDGER89_ENOMEM;
    }
    dir->dirp = d;
    *out = dir;
    return LEDGER89_OK;
}

static int led89_eof_status(void)
{
    if (errno != 0)
    {
        return LEDGER89_EIO;
    }
    return LEDGER89_OK;
}

static struct dirent *led89_readdir_once(DIR *d)
{
    struct dirent *ent;

    errno = 0;
    ent = readdir(d);
    return ent;
}

static struct dirent *led89_readdir_retry(DIR *d)
{
    struct dirent *ent;

    for (;;)
    {
        ent = led89_readdir_once(d);
        if (ent != NULL)
        {
            return ent;
        }
        if (errno != EINTR)
        {
            return ent;
        }
    }
}

static int led89_posix_next_entry(DIR *d, struct dirent **out)
{
    int rc;

    for (;;)
    {
        *out = led89_readdir_retry(d);
        if (*out == NULL)
        {
            rc = led89_eof_status();
            return rc;
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
        return LEDGER89_ERANGE;
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
        return LEDGER89_EIO;
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
                                          led89_posix_list_close,
                                          led89_posix_entropy};

const led89_io *led89_io_posix(void)
{
    return &led89_posix_impl;
}
