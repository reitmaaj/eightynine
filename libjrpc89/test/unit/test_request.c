/* test_request.c - unit tests for jrpc89_request_new. */
#include <stdio.h>
#include <string.h>

#include <jrpc89.h>

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static int render_to(j89_arena *a, j89_len node, char *buf, j89_len cap)
{
    j89_arena out;
    j89_len n;
    j89_len i;
    char *p;
    j89_arena_init(&out);
    if (j89_render(a, node, 1, &out) != 0)
    {
        j89_arena_destroy(&out);
        return -1;
    }
    p = (char *)out.mem;
    n = out.off;
    if (n >= cap)
    {
        j89_arena_destroy(&out);
        return -1;
    }
    i = 0;
    while (i < n)
    {
        buf[i] = p[i];
        i = i + 1;
    }
    buf[n] = '\0';
    j89_arena_destroy(&out);
    return 0;
}

static int expect_render(j89_arena *a, j89_len req, const char *expect)
{
    char buf[256];
    int r;
    r = render_to(a, req, buf, sizeof(buf));
    if (r != 0)
    {
        fail("render failed");
        return 0;
    }
    if (strcmp(buf, expect) != 0)
    {
        fprintf(stderr, "FAIL: got <%s> want <%s>\n", buf, expect);
        failures = failures + 1;
        return 0;
    }
    return 1;
}

static void test_int_id(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 7;
    req = jrpc89_request_new(&a, "foo", J89_BAD, &id);
    if (!jrpc89_has_node(req))
    {
        fail("int id: request_new");
    }
    else
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"foo\",\"id\":7}");
    }
    j89_arena_destroy(&a);
}

static void test_string_id(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_STRING;
    id.str = "abc";
    id.len = 3;
    req = jrpc89_request_new(&a, "foo", J89_BAD, &id);
    if (!jrpc89_has_node(req))
    {
        fail("string id: request_new");
    }
    else
    {
        expect_render(
            &a, req, "{\"jsonrpc\":\"2.0\",\"method\":\"foo\",\"id\":\"abc\"}");
    }
    j89_arena_destroy(&a);
}

static void test_null_id(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_NULL;
    req = jrpc89_request_new(&a, "foo", J89_BAD, &id);
    if (!jrpc89_has_node(req))
    {
        fail("null id: request_new");
    }
    else
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"foo\",\"id\":null}");
    }
    j89_arena_destroy(&a);
}

static void test_notification(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_NONE;
    req = jrpc89_request_new(&a, "foo", J89_BAD, &id);
    if (!jrpc89_has_node(req))
    {
        fail("notification: request_new");
    }
    else
    {
        expect_render(&a, req, "{\"jsonrpc\":\"2.0\",\"method\":\"foo\"}");
    }
    j89_arena_destroy(&a);
}

static void test_params(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len params;
    j89_len req;
    j89_len num;
    j89_arena_init(&a);
    params = j89_object_new(&a, 1);
    num = j89_integer_new(&a, 1);
    j89_object_set(&a, params, 0, "a", 1, num);
    id.kind = JRPC89_ID_INT;
    id.num = 7;
    req = jrpc89_request_new(&a, "foo", params, &id);
    if (!jrpc89_has_node(req))
    {
        fail("params: request_new");
    }
    else
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"foo\","
                      "\"params\":{\"a\":1},\"id\":7}");
    }
    j89_arena_destroy(&a);
}

static void test_empty_method(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 1;
    req = jrpc89_request_new(&a, "", J89_BAD, &id);
    if (jrpc89_has_node(req))
    {
        fail("empty method accepted");
    }
    j89_arena_destroy(&a);
}

static void test_null_method(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 1;
    req = jrpc89_request_new(&a, (const char *)0, J89_BAD, &id);
    if (jrpc89_has_node(req))
    {
        fail("null method accepted");
    }
    j89_arena_destroy(&a);
}

static void test_invalid_utf8_method(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 1;
    req = jrpc89_request_new(&a, "\xff", J89_BAD, &id);
    if (jrpc89_has_node(req))
    {
        fail("invalid utf-8 method returned a node");
    }
    if (!j89_failed(&a))
    {
        fail("invalid utf-8 method did not fail the arena");
    }
    j89_arena_destroy(&a);
}

static void test_prefailed_arena(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    j89_string_new(&a, "\xff", 1);
    if (!j89_failed(&a))
    {
        fail("prefailed: setup did not fail the arena");
    }
    id.kind = JRPC89_ID_INT;
    id.num = 1;
    req = jrpc89_request_new(&a, "foo", J89_BAD, &id);
    if (jrpc89_has_node(req))
    {
        fail("prefailed arena returned a node");
    }
    j89_arena_destroy(&a);
}

int main(void)
{
    test_int_id();
    test_string_id();
    test_null_id();
    test_notification();
    test_params();
    test_empty_method();
    test_null_method();
    test_invalid_utf8_method();
    test_prefailed_arena();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_request: ok\n");
    return 0;
}
