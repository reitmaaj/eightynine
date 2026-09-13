/* io.c - newline-delimited JSON framing over an open socket fd. */
#include <errno.h>
#include <unistd.h>

#include <jrpc89.h>

/* Write up to n bytes, retrying on EINTR. Returns the number of bytes
 * written, or -1 on a hard error. */
static ssize_t jrpc89_write_one(int fd, const char *buf, size_t n)
{
    ssize_t w;
    for (;;)
    {
        w = write(fd, buf, n);
        if (w >= 0)
        {
            break;
        }
        if (errno == EINTR)
        {
            continue;
        }
        break;
    }
    return w;
}

/* One write attempt that advances *off past the bytes written. Returns 0 on
 * progress, -1 on a write error (including EOF). */
static int jrpc89_advance_write(int fd, const char *buf, j89_len len,
                                j89_len *off)
{
    ssize_t w;
    j89_len wlen;
    w = jrpc89_write_one(fd, buf + *off, len - *off);
    if (w <= 0)
    {
        return -1;
    }
    wlen = (j89_len)w;
    *off = *off + wlen;
    return 0;
}

static int jrpc89_write_bytes(int fd, const char *buf, j89_len len)
{
    j89_len off;
    off = 0;
    while (off < len)
    {
        int st;
        st = jrpc89_advance_write(fd, buf, len, &off);
        if (st != 0)
        {
            return -1;
        }
    }
    return 0;
}

int jrpc89_write_frame(int fd, const char *json, j89_len len)
{
    int r;
    int n;
    r = jrpc89_write_bytes(fd, json, len);
    if (r != 0)
    {
        return -1;
    }
    n = jrpc89_write_bytes(fd, "\n", 1);
    if (n != 0)
    {
        return -1;
    }
    return 0;
}

/* Read up to one byte, retrying on EINTR. Returns 1 on a byte, 0 on EOF,
 * or -1 on a hard error. */
static ssize_t jrpc89_read_one(int fd, char *c)
{
    ssize_t w;
    for (;;)
    {
        w = read(fd, c, 1);
        if (w >= 0)
        {
            break;
        }
        if (errno == EINTR)
        {
            continue;
        }
        break;
    }
    return w;
}

/* Fold one received byte into the frame. Returns 0 when the payload keeps
 * filling, nonzero when the frame ends (newline seen or buffer overflow). */
static int jrpc89_ingest_byte(char *buf, j89_len limit, char c, j89_len *i,
                              int *found, int *toobig)
{
    if (c == '\n')
    {
        *found = 1;
        return 1;
    }
    if (*i >= limit)
    {
        *toobig = 1;
        return 1;
    }
    buf[*i] = c;
    *i = *i + 1;
    return 0;
}

/* One read step: pull a byte and fold it into the frame state. Returns 0
 * when reading should continue, nonzero when the frame is complete or
 * aborted. */
static int jrpc89_read_step(int fd, char *buf, j89_len limit, j89_len *i,
                            int *found, int *toobig, int *err)
{
    ssize_t w;
    char c;
    int step;
    w = jrpc89_read_one(fd, &c);
    if (w == 1)
    {
        step = jrpc89_ingest_byte(buf, limit, c, i, found, toobig);
        return step;
    }
    if (w == 0)
    {
        return 1;
    }
    *err = 1;
    return 1;
}

int jrpc89_read_frame(int fd, char *buf, j89_len cap, j89_len *out_len)
{
    j89_len limit;
    j89_len i;
    int found;
    int toobig;
    int err;
    found = 0;
    toobig = 0;
    err = 0;
    if (cap == 0)
    {
        return -1;
    }
    limit = cap - 1;
    i = 0;
    while (found == 0)
    {
        int stop;
        stop = jrpc89_read_step(fd, buf, limit, &i, &found, &toobig, &err);
        if (stop != 0)
        {
            break;
        }
    }
    buf[i] = '\0';
    *out_len = i;
    if (found)
    {
        return 0;
    }
    if (toobig)
    {
        return -2;
    }
    if (err)
    {
        return -3;
    }
    if (i == 0)
    {
        return -1;
    }
    return -4;
}
