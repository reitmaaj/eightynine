/* test_io.c - unit tests for NDJSON framing. */
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <jrpc89.h>

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static void test_roundtrip(void)
{
    int sv[2];
    char buf[128];
    j89_len len;
    int r;
    r = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    if (r != 0)
    {
        fail("socketpair");
        return;
    }
    r = jrpc89_write_frame(sv[0], "{\"a\":1}", 7);
    if (r != 0)
    {
        fail("write_frame");
    }
    r = jrpc89_read_frame(sv[1], buf, sizeof(buf), &len);
    if (r != 0)
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
    int r;
    r = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    if (r != 0)
    {
        fail("socketpair multiple");
        return;
    }
    r = jrpc89_write_frame(sv[0], "one", 3);
    if (r != 0)
    {
        fail("write one");
    }
    r = jrpc89_write_frame(sv[0], "two", 3);
    if (r != 0)
    {
        fail("write two");
    }
    r = jrpc89_read_frame(sv[1], buf, sizeof(buf), &len);
    if (r != 0 || len != 3 || strncmp(buf, "one", 3) != 0)
    {
        fail("read one");
    }
    r = jrpc89_read_frame(sv[1], buf, sizeof(buf), &len);
    if (r != 0 || len != 3 || strncmp(buf, "two", 3) != 0)
    {
        fail("read two");
    }
    close(sv[0]);
    close(sv[1]);
}

static void test_eof_no_data(void)
{
    int sv[2];
    char buf[128];
    j89_len len;
    int r;
    r = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    if (r != 0)
    {
        fail("socketpair eof");
        return;
    }
    close(sv[0]);
    r = jrpc89_read_frame(sv[1], buf, sizeof(buf), &len);
    if (r == 0)
    {
        fail("read after close returned success");
    }
    close(sv[1]);
}

static void test_exact_fit_frame(void)
{
    int sv[2];
    char buf[8];
    j89_len len;
    int r;
    r = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    if (r != 0)
    {
        fail("socketpair exact fit");
        return;
    }
    r = jrpc89_write_frame(sv[0], "abcdefg", 7);
    if (r != 0)
    {
        fail("write exact fit");
    }
    r = jrpc89_read_frame(sv[1], buf, sizeof(buf), &len);
    if (r != 0)
    {
        fprintf(stderr, "FAIL: exact fit read got %d\n", r);
        failures = failures + 1;
    }
    else if (len != 7 || strncmp(buf, "abcdefg", 7) != 0)
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
    int r;
    r = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    if (r != 0)
    {
        fail("socketpair too long");
        return;
    }
    r = jrpc89_write_frame(sv[0], "abcdefgh", 8);
    if (r != 0)
    {
        fail("write too long");
    }
    r = jrpc89_read_frame(sv[1], buf, sizeof(buf), &len);
    if (r != -2)
    {
        fprintf(stderr, "FAIL: too long read got %d\n", r);
        failures = failures + 1;
    }
    close(sv[0]);
    close(sv[1]);
}

static void test_truncated_frame(void)
{
    int sv[2];
    char buf[128];
    j89_len len;
    int r;
    r = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    if (r != 0)
    {
        fail("socketpair truncated");
        return;
    }
    r = (int)write(sv[0], "abc", 3);
    if (r != 3)
    {
        fail("write partial");
    }
    close(sv[0]);
    r = jrpc89_read_frame(sv[1], buf, sizeof(buf), &len);
    if (r != -4)
    {
        fprintf(stderr, "FAIL: truncated read got %d\n", r);
        failures = failures + 1;
    }
    close(sv[1]);
}

int main(void)
{
    test_roundtrip();
    test_multiple_frames();
    test_eof_no_data();
    test_exact_fit_frame();
    test_too_long_frame();
    test_truncated_frame();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_io: ok\n");
    return 0;
}
