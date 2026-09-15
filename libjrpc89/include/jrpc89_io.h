#ifndef JRPC89_IO_H
#define JRPC89_IO_H

#include <jrpc89.h>

/* Optional POSIX transport profile: newline-delimited JSON over an open,
 * blocking, connected Unix-socket fd.
 *
 * The caller owns fd. The library never opens, connects, accepts, or closes
 * it, and simultaneous calls on one fd require external serialization.
 * Nonblocking descriptors are out of scope in V1: EAGAIN/EWOULDBLOCK surface
 * as JRPC89_EIO.
 *
 * SIGPIPE: the adapter suppresses it where the platform offers
 * MSG_NOSIGNAL. On other platforms the application MUST configure its
 * SIGPIPE policy (for example, signal(SIGPIPE, SIG_IGN)) to observe
 * JRPC89_EIO instead of process termination.
 *
 * Partial writes: once any byte reaches fd, a later failure can leave a
 * partial frame; the stream cannot be rolled back. */

/* Write one frame: len bytes plus a trailing newline. Rejects a negative
 * fd, a NULL buffer, a zero-length frame, and any raw '\n' in json with
 * JRPC89_EINVAL, writing nothing. Returns JRPC89_OK on success and
 * JRPC89_EIO on a write failure. */
jrpc89_status jrpc89_fd_write_frame(int fd, const char *json, j89_len len);

/* Read one newline-terminated NDJSON frame into buf (capacity cap bytes).
 * On JRPC89_OK the frame bytes (without the newline) are in buf,
 * NUL-terminated, with *out_len set. A payload of up to cap-1 bytes is
 * accepted.
 *
 * On every non-OK return *out_len is 0 and buf[0] is '\0':
 *   JRPC89_EINVAL   NULL buffer or length pointer, or cap == 0 (untouched)
 *   JRPC89_EOF      EOF before any frame byte
 *   JRPC89_ETRUNC   EOF after payload bytes without a newline
 *   JRPC89_ETOOLONG frame exceeds cap-1; the reader drains through the next
 *                   newline so the next call starts on a frame boundary
 *   JRPC89_EIO      read failure */
jrpc89_status jrpc89_fd_read_frame(int fd, char *buf, j89_len cap,
                                   j89_len *out_len);

#endif
