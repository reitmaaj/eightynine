/* eintr_shim.c - EINTR injection for the POSIX backend via GNU ld --wrap. */

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

#include "eintr_shim.h"

static int armed;
static int op_sel;
static int skip;
static int fired;

void eintr_arm(int op, int skip_count)
{
    armed = 1;
    op_sel = op;
    skip = skip_count;
    fired = 0;
}

int eintr_fired(void)
{
    return fired;
}

static int eintr_hit(int op)
{
    if (armed == 0)
    {
        return 0;
    }
    if (op_sel != op)
    {
        return 0;
    }
    if (skip > 0)
    {
        --skip;
        return 0;
    }
    armed = 0;
    fired = 1;
    errno = EINTR;
    return 1;
}

ssize_t __real_pread(int fd, void *buf, size_t n, off_t off);
ssize_t __wrap_pread(int fd, void *buf, size_t n, off_t off);
ssize_t __wrap_pwrite(int fd, const void *buf, size_t n, off_t off);
int __wrap_fsync(int fd);
int __wrap_fdatasync(int fd);
int __wrap_ftruncate(int fd, off_t len);
int __wrap_rename(const char *from, const char *to);
int __wrap_unlink(const char *path);

ssize_t __wrap_pread(int fd, void *buf, size_t n, off_t off)
{
    if (eintr_hit(EINTR_PREAD) != 0)
    {
        return -1;
    }
    return __real_pread(fd, buf, n, off);
}

ssize_t __real_pwrite(int fd, const void *buf, size_t n, off_t off);
ssize_t __wrap_pwrite(int fd, const void *buf, size_t n, off_t off)
{
    if (eintr_hit(EINTR_PWRITE) != 0)
    {
        return -1;
    }
    return __real_pwrite(fd, buf, n, off);
}

int __real_fsync(int fd);
int __wrap_fsync(int fd)
{
    if (eintr_hit(EINTR_FSYNC) != 0)
    {
        return -1;
    }
    return __real_fsync(fd);
}

int __real_fdatasync(int fd);
int __wrap_fdatasync(int fd)
{
    if (eintr_hit(EINTR_FDATASYNC) != 0)
    {
        return -1;
    }
    return __real_fdatasync(fd);
}

int __real_ftruncate(int fd, off_t len);
int __wrap_ftruncate(int fd, off_t len)
{
    if (eintr_hit(EINTR_FTRUNCATE) != 0)
    {
        return -1;
    }
    return __real_ftruncate(fd, len);
}

int __real_rename(const char *from, const char *to);
int __wrap_rename(const char *from, const char *to)
{
    if (eintr_hit(EINTR_RENAME) != 0)
    {
        return -1;
    }
    return __real_rename(from, to);
}

int __real_unlink(const char *path);
int __wrap_unlink(const char *path)
{
    if (eintr_hit(EINTR_UNLINK) != 0)
    {
        return -1;
    }
    return __real_unlink(path);
}
