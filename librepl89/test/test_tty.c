#define _DEFAULT_SOURCE 1

#include <errno.h>
#include <pty.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "test.h"
#include "repl89_tty.h"

static int g_eintr_once;

static ssize_t eintr_write(int fd, const void *p, size_t n)
{
    if (g_eintr_once == 0) {
        g_eintr_once = 1;
        errno = EINTR;
        return -1;
    }
    return write(fd, p, n);
}

static void test_enter_leave(void)
{
    int master;
    int slave;
    repl89_tty t;
    struct termios before;
    struct termios during;
    struct termios after;
    char buf[32];
    ssize_t n;
    int r;

    master = -1;
    slave = -1;
    r = openpty(&master, &slave, NULL, NULL, NULL);
    rp_check(r == 0, "openpty");
    if (r != 0) {
        return;
    }
    rp_check(tcgetattr(slave, &before) == 0, "tcgetattr before");
    rp_check(repl89_tty_enter(&t, slave, slave) == REPL89_OK, "enter raw");
    rp_check(tcgetattr(slave, &during) == 0, "tcgetattr during");
    rp_check((during.c_lflag & ICANON) == 0, "icanon disabled");
    rp_check((during.c_lflag & ECHO) == 0, "echo disabled");
    rp_check((during.c_lflag & ISIG) == 0, "isig disabled");
    rp_check((during.c_oflag & OPOST) == 0, "opost disabled");

    memset(buf, 0, sizeof buf);
    n = read(master, buf, sizeof buf);
    rp_check(n == 8 && memcmp(buf, "\x1b[?2004h", 8) == 0,
             "paste enable emitted");

    rp_check(repl89_tty_leave(&t) == REPL89_OK, "leave raw");
    memset(buf, 0, sizeof buf);
    n = read(master, buf, sizeof buf);
    rp_check(n == 8 && memcmp(buf, "\x1b[?2004l", 8) == 0,
             "paste disable emitted");
    rp_check(tcgetattr(slave, &after) == 0, "tcgetattr after");
    rp_check(memcmp(&before, &after, sizeof before) == 0,
             "termios restored exactly");
    close(master);
    close(slave);
}

static void test_size(void)
{
    int master;
    int slave;
    repl89_tty t;
    struct winsize ws;
    int r;

    master = -1;
    slave = -1;
    r = openpty(&master, &slave, NULL, NULL, NULL);
    rp_check(r == 0, "openpty size");
    if (r != 0) {
        return;
    }
    memset(&ws, 0, sizeof ws);
    ws.ws_row = 24;
    ws.ws_col = 80;
    rp_check(ioctl(slave, TIOCSWINSZ, &ws) == 0, "set winsize");
    rp_check(repl89_tty_enter(&t, slave, slave) == REPL89_OK, "enter size");
    rp_check(repl89_tty_size(&t) == REPL89_OK, "query winsize");
    rp_check(t.rows == 24 && t.cols == 80, "winsize values");
    repl89_tty_leave(&t);
    close(master);
    close(slave);
}

static void test_io(void)
{
    int master;
    int slave;
    repl89_tty t;
    char buf[16];
    ssize_t n;
    int r;

    master = -1;
    slave = -1;
    r = openpty(&master, &slave, NULL, NULL, NULL);
    rp_check(r == 0, "openpty io");
    if (r != 0) {
        return;
    }
    rp_check(repl89_tty_enter(&t, slave, slave) == REPL89_OK, "enter io");
    memset(buf, 0, sizeof buf);
    n = read(master, buf, sizeof buf);
    rp_check(n == 8 && memcmp(buf, "\x1b[?2004h", 8) == 0,
             "drain paste enable");
    rp_check(write(master, "xy", 2) == 2, "write master");
    n = repl89_tty_read(&t, buf, sizeof buf);
    rp_check(n == 2 && memcmp(buf, "xy", 2) == 0, "read slave");
    rp_check(repl89_tty_write(&t, "z", 1) == 0, "tty write");
    memset(buf, 0, sizeof buf);
    n = read(master, buf, sizeof buf);
    rp_check(n == 1 && buf[0] == 'z', "read master");
    repl89_tty_leave(&t);
    close(master);
    close(slave);
}

static void test_rejections(void)
{
    repl89_tty t;
    int fds[2];

    rp_check(repl89_tty_enter(&t, -1, -1) == REPL89_ETTY, "bad fd rejected");
    rp_check(pipe(fds) == 0, "pipe");
    rp_check(repl89_tty_enter(&t, fds[0], fds[1]) == REPL89_ETTY,
             "pipe rejected");
    rp_check(repl89_tty_leave(&t) == REPL89_OK, "leave without enter");
    close(fds[0]);
    close(fds[1]);
}

static void test_write_eintr(void)
{
    int master;
    int slave;
    repl89_tty t;
    char buf[8];
    ssize_t n;
    int r;

    master = -1;
    slave = -1;
    r = openpty(&master, &slave, NULL, NULL, NULL);
    rp_check(r == 0, "openpty eintr");
    if (r != 0) {
        return;
    }
    rp_check(repl89_tty_enter(&t, slave, slave) == REPL89_OK, "enter eintr");
    memset(buf, 0, sizeof buf);
    n = read(master, buf, sizeof buf);
    rp_check(n == 8 && memcmp(buf, "\x1b[?2004h", 8) == 0, "drain eintr");

    g_eintr_once = 0;
    repl89_tty_set_hooks(NULL, NULL, NULL, NULL, eintr_write);
    rp_check(repl89_tty_write(&t, "z", 1) == 0, "FI-10 EINTR retried");
    repl89_tty_reset_hooks();
    memset(buf, 0, sizeof buf);
    n = read(master, buf, sizeof buf);
    rp_check(n == 1 && buf[0] == 'z', "FI-10 byte written after retry");

    repl89_tty_leave(&t);
    close(master);
    close(slave);
}

void test_tty(void)
{
    test_enter_leave();
    test_size();
    test_io();
    test_write_eintr();
    test_rejections();
}
