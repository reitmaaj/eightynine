/* test_sse.c - unit tests for llm89_buf and the SSE parser. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/llm89_buf.h"
#include "../src/llm89_sse.h"

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

/* ---- buffer tests ---- */

static void test_buf_append_basic(void)
{
    llm_buf b;
    const char *d;
    llm_buf_init(&b, 100);
    llm_buf_append(&b, "hel", 3);
    llm_buf_append(&b, "lo", 2);
    if (llm_buf_len(&b) != 5)
    {
        fail("buf length");
    }
    d = llm_buf_data(&b);
    if (strcmp(d, "hello") != 0)
    {
        fail("buf content");
    }
    if (llm_buf_failed(&b) != 0)
    {
        fail("buf not failed");
    }
    llm_buf_destroy(&b);
}

static void test_buf_reset(void)
{
    llm_buf b;
    llm_buf_init(&b, 100);
    llm_buf_append(&b, "abc", 3);
    llm_buf_reset(&b);
    if (llm_buf_len(&b) != 0)
    {
        fail("buf reset length");
    }
    if (strcmp(llm_buf_data(&b), "") != 0)
    {
        fail("buf reset content");
    }
    llm_buf_destroy(&b);
}

static void test_buf_limit(void)
{
    llm_buf b;
    int r;
    llm_buf_init(&b, 3);
    r = llm_buf_append(&b, "abc", 3);
    if (r != 0)
    {
        fail("buf within limit");
    }
    r = llm_buf_append(&b, "d", 1);
    if (r == 0)
    {
        fail("buf over limit not reported");
    }
    if (llm_buf_failed(&b) == 0)
    {
        fail("buf failed flag");
    }
    llm_buf_destroy(&b);
}

static void test_buf_empty_data_is_terminated(void)
{
    llm_buf b;
    llm_buf_init(&b, 0);
    if (llm_buf_len(&b) != 0)
    {
        fail("empty buf length");
    }
    if (strcmp(llm_buf_data(&b), "") != 0)
    {
        fail("empty buf data");
    }
    llm_buf_destroy(&b);
}

/* ---- SSE tests ---- */

struct collect
{
    char *buf; /* concatenated payloads */
    size_t cap;
    size_t len;
    int events;
    int stop_after; /* if nonzero, emit returns nonzero after this event */
};

static void c_add(struct collect *c, const char *s, size_t n)
{
    if (c->len + n + 1 > c->cap)
    {
        size_t ncap;
        char *nb;
        ncap = c->cap == 0 ? 64 : c->cap * 2;
        while (ncap < c->len + n + 1)
        {
            ncap = ncap * 2;
        }
        nb = (char *)realloc(c->buf, ncap);
        if (nb == NULL)
        {
            return;
        }
        c->buf = nb;
        c->cap = ncap;
    }
    memcpy(c->buf + c->len, s, n);
    c->len = c->len + n;
    c->buf[c->len] = '\0';
}

static int emit_all(void *userdata, const char *payload, size_t len)
{
    struct collect *c;
    c = (struct collect *)userdata;
    c_add(c, "<", 1);
    c_add(c, payload, len);
    c_add(c, ">", 1);
    c->events = c->events + 1;
    return 0;
}

static int emit_stop(void *userdata, const char *payload, size_t len)
{
    struct collect *c;
    c = (struct collect *)userdata;
    c_add(c, "<", 1);
    c_add(c, payload, len);
    c_add(c, ">", 1);
    c->events = c->events + 1;
    if (c->stop_after != 0 && c->events >= c->stop_after)
    {
        return 1;
    }
    return 0;
}

static void feed_partitioned(llm_sse *p, const char *bytes, size_t len,
                             size_t chunk)
{
    size_t i;
    size_t take;
    if (chunk == 0)
    {
        chunk = len;
    }
    i = 0;
    while (i < len)
    {
        take = len - i;
        if (take > chunk)
        {
            take = chunk;
        }
        llm_sse_feed(p, bytes + i, take);
        i = i + take;
    }
}

static int test_feed(const char *label, const char *bytes, size_t chunk,
                     const char *expected, int expected_events)
{
    llm_sse p;
    struct collect c;
    int r;
    memset(&c, 0, sizeof(c));
    llm_sse_init(&p, 1024, &c, emit_all);
    feed_partitioned(&p, bytes, strlen(bytes), chunk);
    r = llm_sse_failed(&p);
    llm_sse_destroy(&p);
    if (r != 0)
    {
        fail(label);
        fprintf(stderr, "  parser failed\n");
        free(c.buf);
        return 1;
    }
    if (c.events != expected_events)
    {
        fail(label);
        fprintf(stderr, "  expected %d events, got %d\n", expected_events,
                c.events);
        free(c.buf);
        return 1;
    }
    if (c.buf == NULL && expected != NULL)
    {
        fail(label);
        fprintf(stderr, "  no output\n");
        return 1;
    }
    if (expected != NULL && strcmp(c.buf, expected) != 0)
    {
        fail(label);
        fprintf(stderr, "  expected <%s>, got <%s>\n", expected,
                c.buf == NULL ? "(null)" : c.buf);
        free(c.buf);
        return 1;
    }
    free(c.buf);
    return 0;
}

static void test_sse_basic(void)
{
    if (test_feed("basic one byte", "data:hello\n\n", 1, "<hello>", 1) != 0)
    {
        return;
    }
    if (test_feed("basic all at once", "data:hello\n\n", 0, "<hello>", 1) != 0)
    {
        return;
    }
}

static void test_sse_line_endings(void)
{
    if (test_feed("lf", "data:a\n\n", 0, "<a>", 1) != 0)
    {
        return;
    }
    if (test_feed("crlf", "data:a\r\n\r\n", 0, "<a>", 1) != 0)
    {
        return;
    }
    if (test_feed("cr", "data:a\r\r", 0, "<a>", 1) != 0)
    {
        return;
    }
}

static void test_sse_split_boundary(void)
{
    if (test_feed("crlf split", "data:a\r", 0, NULL, 0) != 0)
    {
        return;
    }
    {
        llm_sse p;
        struct collect c;
        memset(&c, 0, sizeof(c));
        llm_sse_init(&p, 1024, &c, emit_all);
        llm_sse_feed(&p, "data:a\r", 7);
        llm_sse_feed(&p, "\n\r\n", 3);
        llm_sse_destroy(&p);
        if (c.events != 1 || strcmp(c.buf, "<a>") != 0)
        {
            fail("crlf split across calls");
        }
        free(c.buf);
    }
}

static void test_sse_multiline_data(void)
{
    if (test_feed("multi data", "data:a\ndata:b\n\n", 0, "<a\nb>", 1) != 0)
    {
        return;
    }
}

static void test_sse_strip_one_space(void)
{
    if (test_feed("space stripped", "data: x\n\n", 0, "<x>", 1) != 0)
    {
        return;
    }
    if (test_feed("no space", "data:x\n\n", 0, "<x>", 1) != 0)
    {
        return;
    }
}

static void test_sse_ignore_other_fields(void)
{
    if (test_feed("comments and fields",
                  ":comment\nevent:foo\nid:1\nretry:100\ndata:z\n\n", 0, "<z>",
                  1) != 0)
    {
        return;
    }
}

static void test_sse_empty_event_nothing(void)
{
    if (test_feed("blank only", "\n", 0, NULL, 0) != 0)
    {
        return;
    }
    if (test_feed("data blank no data", "event:x\n\n", 0, NULL, 0) != 0)
    {
        return;
    }
}

static void test_sse_multiple_events(void)
{
    if (test_feed("two events", "data:a\n\ndata:b\n\n", 0, "<a><b>", 2) != 0)
    {
        return;
    }
}

static void test_sse_done_token_is_payload(void)
{
    if (test_feed("done token", "data:[DONE]\n\n", 0, "<[DONE]>", 1) != 0)
    {
        return;
    }
}

static void test_sse_utf8_split(void)
{
    if (test_feed("utf8 split", "data:a\303\251\n\n", 1, "<a\303\251>", 1) != 0)
    {
        return;
    }
}

static void test_sse_crlf_split_multiline(void)
{
    llm_sse p;
    struct collect c;
    memset(&c, 0, sizeof(c));
    llm_sse_init(&p, 1024, &c, emit_all);
    llm_sse_feed(&p, "data:a\r", 7);
    llm_sse_feed(&p, "\ndata:b\r\n\r\n", 12);
    llm_sse_destroy(&p);
    if (c.events != 1 || strcmp(c.buf, "<a\nb>") != 0)
    {
        fail("crlf split across calls must not split one event");
        fprintf(stderr, "  got %d events <%s>\n", c.events,
                c.buf == NULL ? "(null)" : c.buf);
    }
    free(c.buf);
}

static int run_split(const char *bytes, size_t split, struct collect *c,
                     size_t limit)
{
    llm_sse p;
    int r;
    memset(c, 0, sizeof(*c));
    llm_sse_init(&p, limit, c, emit_all);
    r = llm_sse_feed(&p, bytes, split);
    if (r != 0)
    {
        llm_sse_destroy(&p);
        return r;
    }
    r = llm_sse_feed(&p, bytes + split, strlen(bytes) - split);
    if (r != 0)
    {
        llm_sse_destroy(&p);
        return r;
    }
    llm_sse_destroy(&p);
    return 0;
}

static void test_sse_every_split(void)
{
    static const char *fixtures[] = {
        "data:hello\n\ndata:world\n\n",
        "data:one\ndata:two\n\ndata:three\n\ndata:[DONE]\n\n",
        ":comment\ndata:a\r\n\r\n:other\nevent:x\nretry:5\n\ndata:b\r\r",
        "data:\n\ndata:[DONE]\n\n",
    };
    size_t f;
    f = 0;
    while (f < (sizeof(fixtures) / sizeof(fixtures[0])))
    {
        const char *bytes;
        size_t len;
        struct collect expect;
        size_t k;
        bytes = fixtures[f];
        len = strlen(bytes);
        memset(&expect, 0, sizeof(expect));
        {
            llm_sse p;
            llm_sse_init(&p, 1024, &expect, emit_all);
            llm_sse_feed(&p, bytes, len);
            llm_sse_destroy(&p);
        }
        for (k = 0; k <= len; k = k + 1)
        {
            struct collect got;
            if (run_split(bytes, k, &got, 1024) != 0)
            {
                fail("split returned error");
                free(got.buf);
                break;
            }
            if (got.events != expect.events ||
                (expect.buf == NULL && got.buf != NULL) ||
                (expect.buf != NULL && got.buf != NULL &&
                 strcmp(got.buf, expect.buf) != 0))
            {
                fail("split point differs from all-at-once");
                fprintf(stderr,
                        "  fixture %lu split %lu: expected <%s> (%d), "
                        "got <%s> (%d)\n",
                        (unsigned long)f, (unsigned long)k,
                        expect.buf ? expect.buf : "(null)", expect.events,
                        got.buf ? got.buf : "(null)", got.events);
                free(got.buf);
                free(expect.buf);
                return;
            }
            free(got.buf);
        }
        free(expect.buf);
        f = f + 1;
    }
}

static void test_sse_overflow(void)
{
    llm_sse p;
    struct collect c;
    char big[64];
    int i;
    int r;
    memset(big, 'x', sizeof(big));
    memset(&c, 0, sizeof(c));
    llm_sse_init(&p, 32, &c, emit_all);
    for (i = 0; i < 64; i = i + 1)
    {
        r = llm_sse_feed(&p, "data:", 5);
        if (r == 1)
        {
            break;
        }
        r = llm_sse_feed(&p, big, sizeof(big));
        if (r == 1)
        {
            break;
        }
    }
    if (llm_sse_failed(&p) == 0)
    {
        fail("sse overflow not detected");
    }
    llm_sse_destroy(&p);
    free(c.buf);
}

static void test_sse_emit_stop(void)
{
    llm_sse p;
    struct collect c;
    int r;
    memset(&c, 0, sizeof(c));
    c.stop_after = 1;
    llm_sse_init(&p, 1024, &c, emit_stop);
    r = llm_sse_feed(&p, "data:a\n\ndata:b\n\n", 18);
    llm_sse_destroy(&p);
    if (r != -1)
    {
        fail("sse emit stop not propagated");
    }
    if (c.events != 1 || strcmp(c.buf, "<a>") != 0)
    {
        fail("sse emit stop partial delivery");
    }
    free(c.buf);
}

int main(void)
{
    failures = 0;
    test_buf_append_basic();
    test_buf_reset();
    test_buf_limit();
    test_buf_empty_data_is_terminated();
    test_sse_basic();
    test_sse_line_endings();
    test_sse_split_boundary();
    test_sse_crlf_split_multiline();
    test_sse_multiline_data();
    test_sse_strip_one_space();
    test_sse_ignore_other_fields();
    test_sse_empty_event_nothing();
    test_sse_multiple_events();
    test_sse_every_split();
    test_sse_done_token_is_payload();
    test_sse_utf8_split();
    test_sse_overflow();
    test_sse_emit_stop();
    if (failures != 0)
    {
        fprintf(stderr, "test_sse: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_sse: OK\n");
    return 0;
}
