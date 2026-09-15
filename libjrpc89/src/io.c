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

/* True when the bytes contain a raw newline, which would turn one frame
 * into several. */
static int jrpc89_has_newline(const char *json, j89_len len)
{
    j89_len i;
    for (i = 0; i < len; i = i + 1)
    {
        if (json[i] == '\n')
        {
            return 1;
        }
    }
    return 0;
}

jrpc89_status jrpc89_write_frame(int fd, const char *json, j89_len len)
{
    int nl;
    int r;
    if (fd < 0)
    {
        return JRPC89_EINVAL;
    }
    if (json == NULL)
    {
        return JRPC89_EINVAL;
    }
    if (len == 0)
    {
        return JRPC89_EINVAL;
    }
    nl = jrpc89_has_newline(json, len);
    if (nl)
    {
        return JRPC89_EINVAL;
    }
    r = jrpc89_write_bytes(fd, json, len);
    if (r != 0)
    {
        return JRPC89_EIO;
    }
    r = jrpc89_write_bytes(fd, "\n", 1);
    if (r != 0)
    {
        return JRPC89_EIO;
    }
    return JRPC89_OK;
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

/* Fold one received byte into the frame. Returns 0 when reading continues,
 * nonzero when the frame ends (a newline arrived, normally or at the end of
 * an overflow drain). */
static int jrpc89_ingest(char *buf, j89_len limit, char c, j89_len *i,
                         int *found, int *toobig)
{
    if (*toobig)
    {
        if (c == '\n')
        {
            return 1;
        }
        return 0;
    }
    if (c == '\n')
    {
        *found = 1;
        return 1;
    }
    if (*i >= limit)
    {
        *toobig = 1;
        return 0;
    }
    buf[*i] = c;
    *i = *i + 1;
    return 0;
}

/* One read step: pull a byte and fold it into the frame state. Returns 0
 * when reading should continue, nonzero when the frame is complete, the
 * drain finished, or the stream ended. */
static int jrpc89_read_step(int fd, char *buf, j89_len limit, j89_len *i,
                            int *found, int *toobig, int *err, int *eof)
{
    ssize_t w;
    char c;
    int step;
    w = jrpc89_read_one(fd, &c);
    if (w == 1)
    {
        step = jrpc89_ingest(buf, limit, c, i, found, toobig);
        return step;
    }
    if (w == 0)
    {
        *eof = 1;
        return 1;
    }
    *err = 1;
    return 1;
}

/* Clear the caller-visible output after a failed read. */
static void jrpc89_clear_output(char *buf, j89_len *out_len)
{
    *out_len = 0;
    buf[0] = '\0';
}

/* Publish a completed frame. */
static void jrpc89_store_frame(char *buf, j89_len i, j89_len *out_len)
{
    buf[i] = '\0';
    *out_len = i;
}

/* Status for a stream that ended without a newline. */
static jrpc89_status jrpc89_end_status(j89_len i)
{
    if (i == 0)
    {
        return JRPC89_EOF;
    }
    return JRPC89_ETRUNC;
}

jrpc89_status jrpc89_read_frame(int fd, char *buf, j89_len cap,
                                j89_len *out_len)
{
    j89_len limit;
    j89_len i;
    jrpc89_status st;
    int found;
    int toobig;
    int err;
    int eof;
    if (buf == NULL)
    {
        return JRPC89_EINVAL;
    }
    if (out_len == NULL)
    {
        return JRPC89_EINVAL;
    }
    if (cap == 0)
    {
        return JRPC89_EINVAL;
    }
    found = 0;
    toobig = 0;
    err = 0;
    eof = 0;
    limit = cap - 1;
    i = 0;
    while (found == 0)
    {
        int stop;
        stop =
            jrpc89_read_step(fd, buf, limit, &i, &found, &toobig, &err, &eof);
        if (stop != 0)
        {
            break;
        }
    }
    if (found)
    {
        jrpc89_store_frame(buf, i, out_len);
        return JRPC89_OK;
    }
    jrpc89_clear_output(buf, out_len);
    if (err)
    {
        return JRPC89_EIO;
    }
    if (toobig)
    {
        return JRPC89_ETOOLONG;
    }
    if (eof)
    {
        st = jrpc89_end_status(i);
        return st;
    }
    return JRPC89_EIO;
}
