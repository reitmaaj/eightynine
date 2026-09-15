/* test_fault.c - scripted syscall seam tests for NDJSON framing.
 *
 * This test links src/io.c against the seam functions defined below instead
 * of src/io_posix.c, so EINTR, short transfers, EOF, and hard failures are
 * deterministic. */
#include <stdio.h>
#include <string.h>

#include <jrpc89.h>
#include <jrpc89_io.h>

#include "io_internal.h"

#define MAX_ACTIONS 64

struct read_action
{
    int kind;
    char c;
};

struct write_action
{
    int kind;
    j89_len put;
};

static struct read_action rscript[MAX_ACTIONS];
static int rcount;
static int rpos;

static struct write_action wscript[MAX_ACTIONS];
static int wcount;
static int wpos;

static char written[256];
static j89_len written_len;

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static void r_reset(void)
{
    rcount = 0;
    rpos = 0;
}

static void r_byte(char c)
{
    if (rcount < MAX_ACTIONS)
    {
        rscript[rcount].kind = JRPC89_SYS_OK;
        rscript[rcount].c = c;
        rcount = rcount + 1;
    }
}

static void r_kind(int kind)
{
    if (rcount < MAX_ACTIONS)
    {
        rscript[rcount].kind = kind;
        rscript[rcount].c = '\0';
        rcount = rcount + 1;
    }
}

static void w_reset(void)
{
    wcount = 0;
    wpos = 0;
    written_len = 0;
}

static void w_ok(j89_len put)
{
    if (wcount < MAX_ACTIONS)
    {
        wscript[wcount].kind = JRPC89_SYS_OK;
        wscript[wcount].put = put;
        wcount = wcount + 1;
    }
}

static void w_kind(int kind)
{
    if (wcount < MAX_ACTIONS)
    {
        wscript[wcount].kind = kind;
        wscript[wcount].put = 0;
        wcount = wcount + 1;
    }
}

int jrpc89_sys_read(int fd, void *buf, j89_len len, j89_len *got)
{
    struct read_action a;
    char *p;
    (void)fd;
    (void)len;
    if (rpos >= rcount)
    {
        return JRPC89_SYS_EOF;
    }
    a = rscript[rpos];
    rpos = rpos + 1;
    if (a.kind == JRPC89_SYS_OK)
    {
        p = (char *)buf;
        p[0] = a.c;
        *got = 1;
        return JRPC89_SYS_OK;
    }
    return a.kind;
}

int jrpc89_sys_write(int fd, const void *buf, j89_len len, j89_len *put)
{
    struct write_action a;
    const char *p;
    j89_len i;
    j89_len n;
    (void)fd;
    if (wpos >= wcount)
    {
        return JRPC89_SYS_ERR;
    }
    a = wscript[wpos];
    wpos = wpos + 1;
    if (a.kind != JRPC89_SYS_OK)
    {
        return a.kind;
    }
    p = (const char *)buf;
    n = a.put;
    if (n > len)
    {
        n = len;
    }
    for (i = 0; i < n; i = i + 1)
    {
        if (written_len < sizeof(written))
        {
            written[written_len] = p[i];
            written_len = written_len + 1;
        }
    }
    *put = n;
    return JRPC89_SYS_OK;
}

static void test_read_eintr(void)
{
    char buf[8];
    j89_len len;
    jrpc89_status st;
    r_reset();
    r_kind(JRPC89_SYS_INTR);
    r_byte('a');
    r_kind(JRPC89_SYS_INTR);
    r_byte('b');
    r_byte('\n');
    len = 99;
    st = jrpc89_fd_read_frame(3, buf, sizeof(buf), &len);
    if (st != JRPC89_OK)
    {
        fail("read eintr: status");
    }
    else if (len != 2 || strncmp(buf, "ab", 2) != 0)
    {
        fail("read eintr: content");
    }
}

static void test_read_eintr_during_drain(void)
{
    char buf[4];
    j89_len len;
    jrpc89_status st;
    r_reset();
    r_byte('a');
    r_byte('b');
    r_byte('c');
    r_kind(JRPC89_SYS_INTR);
    r_byte('d');
    r_byte('\n');
    r_byte('o');
    r_byte('k');
    r_byte('\n');
    len = 99;
    st = jrpc89_fd_read_frame(3, buf, 3, &len);
    if (st != JRPC89_ETOOLONG)
    {
        fail("drain eintr: first status");
    }
    if (len != 0 || buf[0] != '\0')
    {
        fail("drain eintr: output not cleared");
    }
    st = jrpc89_fd_read_frame(3, buf, 3, &len);
    if (st != JRPC89_OK || len != 2 || strncmp(buf, "ok", 2) != 0)
    {
        fail("drain eintr: recovery");
    }
}

static void test_read_hard_error(void)
{
    char buf[8];
    j89_len len;
    jrpc89_status st;
    r_reset();
    r_kind(JRPC89_SYS_ERR);
    buf[0] = 'x';
    len = 99;
    st = jrpc89_fd_read_frame(3, buf, sizeof(buf), &len);
    if (st != JRPC89_EIO)
    {
        fail("read error: status");
    }
    if (len != 0 || buf[0] != '\0')
    {
        fail("read error: output not cleared");
    }
}

static void test_read_eof(void)
{
    char buf[8];
    j89_len len;
    jrpc89_status st;
    r_reset();
    st = jrpc89_fd_read_frame(3, buf, sizeof(buf), &len);
    if (st != JRPC89_EOF)
    {
        fail("read eof: status");
    }
}

static void test_write_eintr(void)
{
    jrpc89_status st;
    w_reset();
    w_kind(JRPC89_SYS_INTR);
    w_ok(2);
    w_ok(1);
    w_ok(1);
    st = jrpc89_fd_write_frame(3, "abc", 3);
    if (st != JRPC89_OK)
    {
        fail("write eintr: status");
    }
    if (written_len != 4 || strncmp(written, "abc\n", 4) != 0)
    {
        fail("write eintr: bytes");
    }
}

static void test_write_short(void)
{
    jrpc89_status st;
    w_reset();
    w_ok(1);
    w_ok(1);
    w_ok(1);
    w_ok(1);
    st = jrpc89_fd_write_frame(3, "abc", 3);
    if (st != JRPC89_OK)
    {
        fail("write short: status");
    }
    if (written_len != 4 || strncmp(written, "abc\n", 4) != 0)
    {
        fail("write short: bytes");
    }
}

static void test_write_hard_error(void)
{
    jrpc89_status st;
    w_reset();
    w_kind(JRPC89_SYS_ERR);
    st = jrpc89_fd_write_frame(3, "abc", 3);
    if (st != JRPC89_EIO)
    {
        fail("write error: status");
    }
}

static void test_write_partial_then_error(void)
{
    jrpc89_status st;
    w_reset();
    w_ok(1);
    w_kind(JRPC89_SYS_ERR);
    st = jrpc89_fd_write_frame(3, "abc", 3);
    if (st != JRPC89_EIO)
    {
        fail("write partial: status");
    }
    if (written_len != 1 || written[0] != 'a')
    {
        fail("write partial: bytes");
    }
}

static void test_write_rejects_before_seam(void)
{
    jrpc89_status st;
    w_reset();
    st = jrpc89_fd_write_frame(3, "a\nb", 3);
    if (st != JRPC89_EINVAL)
    {
        fail("write newline: status");
    }
    if (wpos != 0)
    {
        fail("write newline: seam reached");
    }
    st = jrpc89_fd_write_frame(3, "abc", 0);
    if (st != JRPC89_EINVAL)
    {
        fail("write empty: status");
    }
    if (wpos != 0)
    {
        fail("write empty: seam reached");
    }
}

int main(void)
{
    test_read_eintr();
    test_read_eintr_during_drain();
    test_read_hard_error();
    test_read_eof();
    test_write_eintr();
    test_write_short();
    test_write_hard_error();
    test_write_partial_then_error();
    test_write_rejects_before_seam();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_fault: ok\n");
    return 0;
}
