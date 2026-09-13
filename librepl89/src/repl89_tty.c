/* repl89_tty.c - POSIX terminal session: raw mode, bracketed paste,
 * TIOCGWINSZ, and read/write. Signal policy (SIGWINCH) stays with the
 * application; this layer only owns terminal configuration and I/O. */

#define _DEFAULT_SOURCE 1

#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "repl89_tty.h"

static const char PASTE_ON[] = "\x1b[?2004h";
static const char PASTE_OFF[] = "\x1b[?2004l";

static int real_getattr(int fd, struct termios *t)
{
    int r;

    r = tcgetattr(fd, t);
    return r;
}

static int real_setattr(int fd, int act, const struct termios *t)
{
    int r;

    r = tcsetattr(fd, act, t);
    return r;
}

static int real_winsize(int fd, struct winsize *ws)
{
    int r;

    r = ioctl(fd, TIOCGWINSZ, ws);
    return r;
}

static ssize_t real_read(int fd, void *p, size_t n)
{
    ssize_t r;

    r = read(fd, p, n);
    return r;
}

static ssize_t real_write(int fd, const void *p, size_t n)
{
    ssize_t r;

    r = write(fd, p, n);
    return r;
}

static int (*g_getattr)(int, struct termios *) = real_getattr;
static int (*g_setattr)(int, int, const struct termios *) = real_setattr;
static int (*g_winsize)(int, struct winsize *) = real_winsize;
static ssize_t (*g_read)(int, void *, size_t) = real_read;
static ssize_t (*g_write)(int, const void *, size_t) = real_write;

void repl89_tty_set_hooks(int (*getattr)(int, struct termios *),
                          int (*setattr)(int, int, const struct termios *),
                          int (*winsize)(int, struct winsize *),
                          ssize_t (*rd)(int, void *, size_t),
                          ssize_t (*wr)(int, const void *, size_t))
{
    g_getattr = real_getattr;
    if (getattr != NULL)
    {
        g_getattr = getattr;
    }
    g_setattr = real_setattr;
    if (setattr != NULL)
    {
        g_setattr = setattr;
    }
    g_winsize = real_winsize;
    if (winsize != NULL)
    {
        g_winsize = winsize;
    }
    g_read = real_read;
    if (rd != NULL)
    {
        g_read = rd;
    }
    g_write = real_write;
    if (wr != NULL)
    {
        g_write = wr;
    }
}

void repl89_tty_reset_hooks(void)
{
    g_getattr = real_getattr;
    g_setattr = real_setattr;
    g_winsize = real_winsize;
    g_read = real_read;
    g_write = real_write;
}

static int write_step(const repl89_tty *t, const char *p, size_t n, size_t *off,
                      int *failed)
{
    ssize_t r;

    if (*off >= n)
    {
        return 0;
    }
    r = g_write(t->out_fd, p + *off, n - *off);
    if (r < 0)
    {
        if (errno == EINTR)
        {
            return 1;
        }
        *failed = 1;
        return 0;
    }
    if (r == 0)
    {
        *failed = 1;
        return 0;
    }
    *off = *off + (size_t)r;
    return 1;
}

int repl89_tty_write(const repl89_tty *t, const char *p, size_t n)
{
    size_t off;
    int failed;
    int go;

    off = 0;
    failed = 0;
    go = 1;
    while (go != 0)
    {
        go = write_step(t, p, n, &off, &failed);
    }
    if (failed != 0)
    {
        return -1;
    }
    return 0;
}

static void tty_paste(const repl89_tty *t, int on)
{
    if (on)
    {
        repl89_tty_write(t, PASTE_ON, sizeof PASTE_ON - 1);
    }
    else
    {
        repl89_tty_write(t, PASTE_OFF, sizeof PASTE_OFF - 1);
    }
}

repl89_error repl89_tty_enter(repl89_tty *t, int in_fd, int out_fd)
{
    struct termios raw;
    int r;
    int w;

    t->in_fd = in_fd;
    t->out_fd = out_fd;
    t->raw = 0;
    t->rows = 0;
    t->cols = 0;
    r = g_getattr(in_fd, &t->saved);
    if (r != 0)
    {
        return REPL89_ETTY;
    }
    raw = t->saved;
    raw.c_iflag =
        raw.c_iflag & (tcflag_t) ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag = raw.c_oflag & (tcflag_t)~OPOST;
    raw.c_cflag = raw.c_cflag | (tcflag_t)CS8;
    raw.c_lflag = raw.c_lflag & (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    r = g_setattr(in_fd, TCSANOW, &raw);
    if (r != 0)
    {
        return REPL89_ETTY;
    }
    t->raw = 1;
    w = repl89_tty_write(t, PASTE_ON, sizeof PASTE_ON - 1);
    if (w != 0)
    {
        repl89_tty_leave(t);
        return REPL89_EIO;
    }
    return REPL89_OK;
}

repl89_error repl89_tty_leave(repl89_tty *t)
{
    int r;
    repl89_error err;

    if (t->raw == 0)
    {
        return REPL89_OK;
    }
    tty_paste(t, 0);
    r = g_setattr(t->in_fd, TCSANOW, &t->saved);
    err = REPL89_OK;
    if (r != 0)
    {
        err = REPL89_ETTY;
    }
    t->raw = 0;
    return err;
}

repl89_error repl89_tty_size(repl89_tty *t)
{
    struct winsize ws;
    int r;

    r = g_winsize(t->out_fd, &ws);
    if (r != 0)
    {
        return REPL89_EIO;
    }
    if (ws.ws_col == 0)
    {
        return REPL89_EIO;
    }
    t->cols = ws.ws_col;
    t->rows = ws.ws_row;
    return REPL89_OK;
}

ssize_t repl89_tty_read(const repl89_tty *t, char *p, size_t n)
{
    ssize_t r;

    r = g_read(t->in_fd, p, n);
    return r;
}
