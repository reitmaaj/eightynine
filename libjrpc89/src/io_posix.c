/* io_posix.c - POSIX read/write adapter behind the framing syscall seam. */
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <jrpc89.h>

#include "io_internal.h"

/* Write one buffer, preferring send(MSG_NOSIGNAL) where the platform
 * provides it so a disconnected peer surfaces as an error rather than
 * SIGPIPE. */
static ssize_t jrpc89_raw_write(int fd, const void *buf, size_t len)
{
    ssize_t w;
#ifdef MSG_NOSIGNAL
    w = send(fd, buf, len, MSG_NOSIGNAL);
#else
    w = write(fd, buf, len);
#endif
    return w;
}

/* Store a positive transfer count. */
static void jrpc89_store_count(j89_len *out, ssize_t v)
{
    *out = (j89_len)v;
}

int jrpc89_sys_read(int fd, void *buf, j89_len len, j89_len *got)
{
    ssize_t r;
    r = read(fd, buf, len);
    if (r > 0)
    {
        jrpc89_store_count(got, r);
        return JRPC89_SYS_OK;
    }
    if (r == 0)
    {
        return JRPC89_SYS_EOF;
    }
    if (errno == EINTR)
    {
        return JRPC89_SYS_INTR;
    }
    return JRPC89_SYS_ERR;
}

int jrpc89_sys_write(int fd, const void *buf, j89_len len, j89_len *put)
{
    ssize_t w;
    w = jrpc89_raw_write(fd, buf, len);
    if (w > 0)
    {
        jrpc89_store_count(put, w);
        return JRPC89_SYS_OK;
    }
    if (w == 0)
    {
        return JRPC89_SYS_ERR;
    }
    if (errno == EINTR)
    {
        return JRPC89_SYS_INTR;
    }
    return JRPC89_SYS_ERR;
}
