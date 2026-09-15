#ifndef JRPC89_IO_INTERNAL_H
#define JRPC89_IO_INTERNAL_H

#include <jrpc89.h>

/* Outcome of one raw read or write attempt. The adapter never retries; the
 * framing logic owns EINTR handling and short-transfer looping. */
enum
{
    JRPC89_SYS_OK = 0, /* bytes transferred */
    JRPC89_SYS_INTR,   /* interrupted; retry */
    JRPC89_SYS_EOF,    /* read side closed (reads only) */
    JRPC89_SYS_ERR     /* hard failure */
};

/* One raw read attempt. On JRPC89_SYS_OK, *got is at least 1. */
int jrpc89_sys_read(int fd, void *buf, j89_len len, j89_len *got);

/* One raw write attempt. On JRPC89_SYS_OK, *put is at least 1. */
int jrpc89_sys_write(int fd, const void *buf, j89_len len, j89_len *put);

#endif
