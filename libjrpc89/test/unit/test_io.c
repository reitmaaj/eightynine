/* test_io.c - unit tests for NDJSON framing. */
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <jrpc89.h>
#include <jrpc89_io.h>

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static int make_pair(int sv[2])
{
    int r;
    r = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    if (r != 0)
    {
        fail("socketpair");
        return -1;
    }
    return 0;
}

static void test_roundtrip(void)
{
    int sv[2];
    char buf[128];
    j89_len len;
    jrpc89_status st;
    if (make_pair(sv) != 0)
    {
        return;
    }
    st = jrpc89_fd_write_frame(sv[0], "{\"a\":1}", 7);
    if (st != JRPC89_OK)
    {
        fail("write_frame");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), &len);
    if (st != JRPC89_OK)
    {
        fail("read_frame");
    }
    if (len != 7)
    {
        fprintf(stderr, "FAIL: len got %lu\n", (unsigned long)len);
        failures = failures + 1;
    }
    if (strncmp(buf, "{\"a\":1}", 7) != 0)
    {
        fail("roundtrip content");
    }
    close(sv[0]);
    close(sv[1]);
}

static void test_multiple_frames(void)
{
    int sv[2];
    char buf[128];
    j89_len len;
    jrpc89_status st;
    if (make_pair(sv) != 0)
    {
        return;
    }
    st = jrpc89_fd_write_frame(sv[0], "one", 3);
    if (st != JRPC89_OK)
    {
        fail("write one");
    }
    st = jrpc89_fd_write_frame(sv[0], "two", 3);
    if (st != JRPC89_OK)
    {
        fail("write two");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), &len);
    if (st != JRPC89_OK || len != 3 || strncmp(buf, "one", 3) != 0)
    {
        fail("read one");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), &len);
    if (st != JRPC89_OK || len != 3 || strncmp(buf, "two", 3) != 0)
    {
        fail("read two");
    }
    close(sv[0]);
    close(sv[1]);
}

static void test_empty_line(void)
{
    int sv[2];
    char buf[8];
    j89_len len;
    jrpc89_status st;
    ssize_t n;
    if (make_pair(sv) != 0)
    {
        return;
    }
    n = write(sv[0], "\n", 1);
    if (n != 1)
    {
        fail("empty line: raw write");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), &len);
    if (st != JRPC89_OK)
    {
        fail("empty line: status");
    }
    if (len != 0)
    {
        fail("empty line: length");
    }
    close(sv[0]);
    close(sv[1]);
}

static void test_eof_no_data(void)
{
    int sv[2];
    char buf[8];
    j89_len len;
    jrpc89_status st;
    if (make_pair(sv) != 0)
    {
        return;
    }
    close(sv[0]);
    buf[0] = 'x';
    len = 7;
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), &len);
    if (st != JRPC89_EOF)
    {
        fail("eof status");
    }
    if (len != 0 || buf[0] != '\0')
    {
        fail("eof output not cleared");
    }
    close(sv[1]);
}

static void test_truncated_frame(void)
{
    int sv[2];
    char buf[128];
    j89_len len;
    jrpc89_status st;
    ssize_t n;
    if (make_pair(sv) != 0)
    {
        return;
    }
    n = write(sv[0], "abc", 3);
    if (n != 3)
    {
        fail("truncated: raw write");
    }
    close(sv[0]);
    buf[0] = 'x';
    len = 7;
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), &len);
    if (st != JRPC89_ETRUNC)
    {
        fail("truncated status");
    }
    if (len != 0 || buf[0] != '\0')
    {
        fail("truncated output not cleared");
    }
    close(sv[1]);
}

static void test_exact_fit_frame(void)
{
    int sv[2];
    char buf[8];
    j89_len len;
    jrpc89_status st;
    if (make_pair(sv) != 0)
    {
        return;
    }
    st = jrpc89_fd_write_frame(sv[0], "abcdefg", 7);
    if (st != JRPC89_OK)
    {
        fail("write exact fit");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), &len);
    if (st != JRPC89_OK)
    {
        fail("exact fit status");
    }
    if (len != 7 || strncmp(buf, "abcdefg", 7) != 0)
    {
        fail("exact fit content");
    }
    close(sv[0]);
    close(sv[1]);
}

static void test_too_long_frame(void)
{
    int sv[2];
    char buf[8];
    j89_len len;
    jrpc89_status st;
    if (make_pair(sv) != 0)
    {
        return;
    }
    st = jrpc89_fd_write_frame(sv[0], "abcdefgh", 8);
    if (st != JRPC89_OK)
    {
        fail("write too long");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), &len);
    if (st != JRPC89_ETOOLONG)
    {
        fail("too long status");
    }
    if (len != 0 || buf[0] != '\0')
    {
        fail("too long output not cleared");
    }
    close(sv[0]);
    close(sv[1]);
}

static void test_oversize_recovery(void)
{
    int sv[2];
    char buf[8];
    j89_len len;
    jrpc89_status st;
    if (make_pair(sv) != 0)
    {
        return;
    }
    st = jrpc89_fd_write_frame(sv[0], "abcdefghijkl", 12);
    if (st != JRPC89_OK)
    {
        fail("recovery: write oversized");
    }
    st = jrpc89_fd_write_frame(sv[0], "end", 3);
    if (st != JRPC89_OK)
    {
        fail("recovery: write valid");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), &len);
    if (st != JRPC89_ETOOLONG)
    {
        fail("recovery: first status");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), &len);
    if (st != JRPC89_OK)
    {
        fail("recovery: second status");
    }
    if (len != 3 || strncmp(buf, "end", 3) != 0)
    {
        fail("recovery: second content");
    }
    close(sv[0]);
    close(sv[1]);
}

static void test_cap_one(void)
{
    int sv[2];
    char buf[1];
    j89_len len;
    jrpc89_status st;
    ssize_t n;
    if (make_pair(sv) != 0)
    {
        return;
    }
    st = jrpc89_fd_write_frame(sv[0], "a", 1);
    if (st != JRPC89_OK)
    {
        fail("cap one: write");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, 1, &len);
    if (st != JRPC89_ETOOLONG)
    {
        fail("cap one: overflow status");
    }
    n = write(sv[0], "\n", 1);
    if (n != 1)
    {
        fail("cap one: raw write");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, 1, &len);
    if (st != JRPC89_OK || len != 0)
    {
        fail("cap one: empty frame");
    }
    close(sv[0]);
    close(sv[1]);
}

static void test_write_rejections(void)
{
    int sv[2];
    char buf[8];
    j89_len len;
    jrpc89_status st;
    if (make_pair(sv) != 0)
    {
        return;
    }
    st = jrpc89_fd_write_frame(sv[0], "a\nb", 3);
    if (st != JRPC89_EINVAL)
    {
        fail("write embedded newline accepted");
    }
    st = jrpc89_fd_write_frame(sv[0], "abc", 0);
    if (st != JRPC89_EINVAL)
    {
        fail("write empty accepted");
    }
    st = jrpc89_fd_write_frame(sv[0], (const char *)0, 3);
    if (st != JRPC89_EINVAL)
    {
        fail("write NULL accepted");
    }
    st = jrpc89_fd_write_frame(-1, "abc", 3);
    if (st != JRPC89_EINVAL)
    {
        fail("write bad fd accepted");
    }
    close(sv[0]);
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), &len);
    if (st != JRPC89_EOF)
    {
        fail("rejected write emitted bytes");
    }
    close(sv[1]);
}

static void test_read_rejections(void)
{
    int sv[2];
    char buf[8];
    j89_len len;
    jrpc89_status st;
    if (make_pair(sv) != 0)
    {
        return;
    }
    buf[0] = 'x';
    len = 7;
    st = jrpc89_fd_read_frame(sv[1], (char *)0, sizeof(buf), &len);
    if (st != JRPC89_EINVAL)
    {
        fail("read NULL buf accepted");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, sizeof(buf), (j89_len *)0);
    if (st != JRPC89_EINVAL)
    {
        fail("read NULL out_len accepted");
    }
    st = jrpc89_fd_read_frame(sv[1], buf, 0, &len);
    if (st != JRPC89_EINVAL)
    {
        fail("read cap zero accepted");
    }
    if (len != 7 || buf[0] != 'x')
    {
        fail("read rejection touched output");
    }
    close(sv[0]);
    close(sv[1]);
}

int main(void)
{
    test_roundtrip();
    test_multiple_frames();
    test_empty_line();
    test_eof_no_data();
    test_truncated_frame();
    test_exact_fit_frame();
    test_too_long_frame();
    test_oversize_recovery();
    test_cap_one();
    test_write_rejections();
    test_read_rejections();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_io: ok\n");
    return 0;
}
