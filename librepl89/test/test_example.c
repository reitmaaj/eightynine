/* test_example.c - fork/exec acceptance tests for the echo example. */

#define _DEFAULT_SOURCE 1

#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "test.h"

#define EX_CAP 16384
#define EX_TIMEOUT_SEC 5
#define EX_REAP_TRIES 500

typedef struct ex_capture {
    int fd;
    char buf[EX_CAP];
    size_t len;
    size_t from;
} ex_capture;

static int ex_find(const char *hay, size_t n, const char *needle, size_t from,
                   size_t *at)
{
    size_t m;
    size_t i;

    m = strlen(needle);
    if (m == 0)
    {
        return 0;
    }
    i = from;
    while (i + m <= n)
    {
        if (memcmp(hay + i, needle, m) == 0)
        {
            *at = i;
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

static size_t ex_count(const char *hay, size_t n, const char *needle)
{
    size_t at;
    size_t from;
    size_t total;

    at = 0;
    from = 0;
    total = 0;
    while (ex_find(hay, n, needle, from, &at))
    {
        total = total + 1;
        from = at + strlen(needle);
    }
    return total;
}

static void ex_capture_init(ex_capture *c, int fd)
{
    c->fd = fd;
    c->len = 0;
    c->from = 0;
    c->buf[0] = 0;
}

static int ex_wait(ex_capture *c, const char *needle)
{
    fd_set set;
    struct timeval tv;
    ssize_t n;
    size_t at;
    size_t room;
    int r;

    for (;;)
    {
        if (ex_find(c->buf, c->len, needle, c->from, &at))
        {
            c->from = at + strlen(needle);
            return 1;
        }
        if (c->len + 1 >= sizeof c->buf)
        {
            return 0;
        }
        FD_ZERO(&set);
        FD_SET(c->fd, &set);
        tv.tv_sec = EX_TIMEOUT_SEC;
        tv.tv_usec = 0;
        r = select(c->fd + 1, &set, NULL, NULL, &tv);
        if (r <= 0)
        {
            return 0;
        }
        room = sizeof c->buf - c->len - 1;
        n = read(c->fd, c->buf + c->len, room);
        if (n <= 0)
        {
            return 0;
        }
        c->len = c->len + (size_t)n;
        c->buf[c->len] = 0;
    }
}

static void ex_send(int fd, const char *p, size_t n)
{
    size_t off;
    ssize_t w;

    off = 0;
    while (off < n)
    {
        w = write(fd, p + off, n - off);
        if (w <= 0)
        {
            break;
        }
        off = off + (size_t)w;
    }
    rp_check(off == n, "EX send bytes");
}

static void ex_child(const char *path, int master, int slave)
{
    close(master);
    if (setsid() < 0)
    {
        _exit(126);
    }
    ioctl(slave, TIOCSCTTY, 0);
    dup2(slave, STDIN_FILENO);
    dup2(slave, STDOUT_FILENO);
    dup2(slave, STDERR_FILENO);
    if (slave > STDERR_FILENO)
    {
        close(slave);
    }
    execl(path, path, (char *)NULL);
    _exit(127);
}

static void ex_cook(int slave)
{
    struct termios tio;

    if (tcgetattr(slave, &tio) == 0)
    {
        tio.c_oflag = tio.c_oflag | OPOST | ONLCR;
        tcsetattr(slave, TCSANOW, &tio);
    }
}

static pid_t ex_spawn(const char *path, int *master_out)
{
    struct winsize ws;
    pid_t pid;
    int master;
    int slave;

    master = -1;
    slave = -1;
    if (openpty(&master, &slave, NULL, NULL, NULL) != 0)
    {
        return -1;
    }
    memset(&ws, 0, sizeof ws);
    ws.ws_row = 24;
    ws.ws_col = 40;
    ioctl(slave, TIOCSWINSZ, &ws);
    ex_cook(slave);
    pid = fork();
    if (pid == 0)
    {
        ex_child(path, master, slave);
    }
    close(slave);
    if (pid < 0)
    {
        close(master);
        return -1;
    }
    *master_out = master;
    return pid;
}

static int ex_reap(pid_t pid, int *status)
{
    pid_t r;
    int i;

    for (i = 0; i < EX_REAP_TRIES; ++i)
    {
        r = waitpid(pid, status, WNOHANG);
        if (r == pid)
        {
            return 1;
        }
        usleep(10000);
    }
    kill(pid, SIGKILL);
    waitpid(pid, status, 0);
    return 0;
}

static void test_example_echo(void)
{
    ex_capture cap;
    int master;
    int status;
    pid_t pid;

    pid = ex_spawn("./build/echo", &master);
    rp_check(pid > 0, "EX-01 spawn");
    if (pid <= 0)
    {
        return;
    }
    ex_capture_init(&cap, master);
    rp_check(ex_wait(&cap, "> ") == 1, "EX-01 prompt");
    ex_send(master, "hello\r", 6);
    rp_check(ex_wait(&cap, "hello\r\n") == 1, "EX-02 exact echo");
    rp_check(ex_wait(&cap, "> ") == 1, "EX-03 next prompt");
    ex_send(master, "\x04", 1);
    rp_check(ex_reap(pid, &status) == 1, "EX-04 EOF exit");
    rp_check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "EX-04 status 0");
    close(master);
}

static void test_example_multiline_utf8(void)
{
    ex_capture cap;
    int master;
    int status;
    pid_t pid;

    pid = ex_spawn("./build/echo", &master);
    rp_check(pid > 0, "EX-02 spawn");
    if (pid <= 0)
    {
        return;
    }
    ex_capture_init(&cap, master);
    rp_check(ex_wait(&cap, "> ") == 1, "EX-02 prompt");
    ex_send(master, "h\xC3\xA9llo\x0Aworld\r", 13);
    rp_check(ex_wait(&cap, "h\xC3\xA9llo\r\nworld\r\n") == 1,
             "EX-02 multiline UTF-8 echo");
    rp_check(ex_wait(&cap, "> ") == 1, "EX-03 next prompt");
    ex_send(master, "\x04", 1);
    rp_check(ex_reap(pid, &status) == 1, "EX-02 EOF exit");
    close(master);
}

static void test_example_cancel(void)
{
    ex_capture cap;
    int master;
    int status;
    pid_t pid;

    pid = ex_spawn("./build/echo", &master);
    rp_check(pid > 0, "EX-05 spawn");
    if (pid <= 0)
    {
        return;
    }
    ex_capture_init(&cap, master);
    rp_check(ex_wait(&cap, "> ") == 1, "EX-05 prompt");
    ex_send(master, "junk\x03", 5);
    rp_check(ex_wait(&cap, "^C") == 1, "EX-03 cancel marker");
    rp_check(ex_count(cap.buf, cap.len, "junk\r\n") == 0, "EX-05 no echo");
    rp_check(ex_wait(&cap, "> ") == 1, "EX-03 next prompt");
    ex_send(master, "\x04", 1);
    rp_check(ex_reap(pid, &status) == 1, "EX-05 EOF exit");
    rp_check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "EX-05 status 0");
    close(master);
}

static void test_example_history(void)
{
    ex_capture cap;
    int master;
    int status;
    pid_t pid;

    pid = ex_spawn("./build/echo", &master);
    rp_check(pid > 0, "EX-04 spawn");
    if (pid <= 0)
    {
        return;
    }
    ex_capture_init(&cap, master);
    rp_check(ex_wait(&cap, "> ") == 1, "EX-04 prompt");
    ex_send(master, "first\r", 6);
    rp_check(ex_wait(&cap, "first\r\n") == 1, "EX-02 first echo");
    rp_check(ex_wait(&cap, "> ") == 1, "EX-03 next prompt");
    ex_send(master, "\x1b[A\r", 4);
    rp_check(ex_wait(&cap, "first\r\n") == 1, "EX-04 history echo");
    rp_check(ex_count(cap.buf, cap.len, "first\r\n") == 2, "EX-04 recalled");
    rp_check(ex_wait(&cap, "> ") == 1, "EX-03 next prompt");
    ex_send(master, "\x04", 1);
    rp_check(ex_reap(pid, &status) == 1, "EX-04 EOF exit");
    close(master);
}

static void test_example_events_resize(void)
{
    ex_capture cap;
    struct winsize ws;
    int master;
    int status;
    pid_t pid;

    pid = ex_spawn("./build/echo_events", &master);
    rp_check(pid > 0, "EX-07 spawn");
    if (pid <= 0)
    {
        return;
    }
    ex_capture_init(&cap, master);
    rp_check(ex_wait(&cap, "> ") == 1, "EX-07 prompt");
    ex_send(master, "hello\r", 6);
    rp_check(ex_wait(&cap, "hello\r\n") == 1, "EX-07 echo");
    rp_check(ex_wait(&cap, "> ") == 1, "EX-07 prompt 2");

    memset(&ws, 0, sizeof ws);
    ws.ws_row = 24;
    ws.ws_col = 30;
    rp_check(ioctl(master, TIOCSWINSZ, &ws) == 0, "EX-07 resize ioctl");
    usleep(100000);
    rp_check(kill(pid, SIGWINCH) == 0, "EX-07 SIGWINCH");
    rp_check(ex_wait(&cap, "> ") == 1, "EX-07 SIGWINCH redraw");

    ex_send(master, "world\r", 6);
    rp_check(ex_wait(&cap, "world\r\n") == 1, "EX-07 after resize");
    rp_check(ex_wait(&cap, "> ") == 1, "EX-07 prompt 3");
    ex_send(master, "\x04", 1);
    rp_check(ex_reap(pid, &status) == 1, "EX-07 EOF exit");
    rp_check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "EX-07 status 0");
    close(master);
}

void test_example(void)
{
    test_example_echo();
    test_example_multiline_utf8();
    test_example_cancel();
    test_example_history();
    test_example_events_resize();
}
