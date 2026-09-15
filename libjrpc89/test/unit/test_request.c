/* test_request.c - unit tests for jrpc89_request_new. */
#include <math.h>
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

static j89_len parse(const char *s, j89_arena *a)
{
    j89_len root;
    root = j89_parse(s, (j89_len)strlen(s), a);
    return root;
}

/* Build with an integer id 7 and assert JRPC89_OK. */
static j89_len build_simple(j89_arena *a, const char *method,
                            j89_len method_len, j89_len params)
{
    jrpc89_id id;
    j89_len req;
    jrpc89_status st;
    id.kind = JRPC89_ID_INT;
    id.num = 7;
    req = 12345;
    st = jrpc89_request_new(a, method, method_len, params, &id, &req);
    if (st != JRPC89_OK)
    {
        fail("build_simple status");
        return J89_BAD;
    }
    return req;
}

static void test_int_id(void)
{
    j89_arena a;
    j89_len req;
    j89_arena_init(&a);
    req = build_simple(&a, "foo", 3, J89_BAD);
    if (req != J89_BAD)
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
    jrpc89_status st;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_STRING;
    id.str = "abc";
    id.len = 3;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_OK)
    {
        fail("string id: status");
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
    jrpc89_status st;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_NULL;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_OK)
    {
        fail("null id: status");
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
    jrpc89_status st;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_NONE;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_OK)
    {
        fail("notification: status");
    }
    else
    {
        expect_render(&a, req, "{\"jsonrpc\":\"2.0\",\"method\":\"foo\"}");
    }
    j89_arena_destroy(&a);
}

static void test_object_params(void)
{
    j89_arena a;
    j89_len params;
    j89_len req;
    j89_len num;
    j89_arena_init(&a);
    params = j89_object_new(&a, 1);
    num = j89_integer_new(&a, 1);
    j89_object_set(&a, params, 0, "a", 1, num);
    req = build_simple(&a, "foo", 3, params);
    if (req != J89_BAD)
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"foo\","
                      "\"params\":{\"a\":1},\"id\":7}");
    }
    j89_arena_destroy(&a);
}

static void test_array_params(void)
{
    j89_arena a;
    j89_len params;
    j89_len req;
    j89_arena_init(&a);
    params = j89_array_new(&a, 1);
    j89_array_set(&a, params, 0, j89_integer_new(&a, 5));
    req = build_simple(&a, "foo", 3, params);
    if (req != J89_BAD)
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"foo\","
                      "\"params\":[5],\"id\":7}");
    }
    j89_arena_destroy(&a);
}

static void test_scalar_params(void)
{
    static const char *texts[] = {"null", "true", "false", "0", "1.5", "\"x\""};
    size_t i;
    for (i = 0; i < sizeof(texts) / sizeof(texts[0]); i = i + 1)
    {
        j89_arena a;
        jrpc89_id id;
        j89_len params;
        j89_len req;
        jrpc89_status st;
        j89_arena_init(&a);
        params = parse(texts[i], &a);
        if (params == J89_BAD)
        {
            fail("scalar params: parse");
        }
        else
        {
            id.kind = JRPC89_ID_INT;
            id.num = 7;
            req = 12345;
            st = jrpc89_request_new(&a, "foo", 3, params, &id, &req);
            if (st != JRPC89_EINVAL)
            {
                fprintf(stderr, "FAIL: scalar params status %d: %s\n", (int)st,
                        texts[i]);
                failures = failures + 1;
            }
            if (req != 12345)
            {
                fail("scalar params: out changed");
            }
        }
        j89_arena_destroy(&a);
    }
}

static void test_empty_method(void)
{
    j89_arena a;
    j89_len req;
    j89_arena_init(&a);
    req = build_simple(&a, "", 0, J89_BAD);
    if (req != J89_BAD)
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"\",\"id\":7}");
    }
    j89_arena_destroy(&a);
}

static void test_null_method_zero_len(void)
{
    j89_arena a;
    j89_len req;
    j89_arena_init(&a);
    req = build_simple(&a, (const char *)0, 0, J89_BAD);
    if (req != J89_BAD)
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"\",\"id\":7}");
    }
    j89_arena_destroy(&a);
}

static void test_null_method_nonzero_len(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    jrpc89_status st;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 7;
    req = 12345;
    st = jrpc89_request_new(&a, (const char *)0, 3, J89_BAD, &id, &req);
    if (st != JRPC89_EINVAL)
    {
        fail("null method with nonzero length: status");
    }
    if (req != 12345)
    {
        fail("null method with nonzero length: out changed");
    }
    j89_arena_destroy(&a);
}

static void test_embedded_nul_method(void)
{
    j89_arena a;
    j89_len req;
    static const char method[] = {'a', '\0', 'b'};
    j89_arena_init(&a);
    req = build_simple(&a, method, 3, J89_BAD);
    if (req != J89_BAD)
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"a\\u0000b\","
                      "\"id\":7}");
    }
    j89_arena_destroy(&a);
}

static void test_embedded_nul_string_id(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    jrpc89_status st;
    static const char bytes[] = {'x', '\0', 'y'};
    j89_arena_init(&a);
    id.kind = JRPC89_ID_STRING;
    id.str = bytes;
    id.len = 3;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_OK)
    {
        fail("embedded nul string id: status");
    }
    else
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"foo\","
                      "\"id\":\"x\\u0000y\"}");
    }
    j89_arena_destroy(&a);
}

static void test_null_arguments(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    jrpc89_status st;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 7;
    req = 12345;
    st = jrpc89_request_new((j89_arena *)0, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_EINVAL)
    {
        fail("null arena: status");
    }
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, (j89_len *)0);
    if (st != JRPC89_EINVAL)
    {
        fail("null out: status");
    }
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, (const jrpc89_id *)0, &req);
    if (st != JRPC89_EINVAL)
    {
        fail("null id: status");
    }
    if (req != 12345)
    {
        fail("null arguments: out changed");
    }
    j89_arena_destroy(&a);
}

static void test_illegal_id_kind(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    jrpc89_status st;
    j89_arena_init(&a);
    id.kind = (jrpc89_id_kind)99;
    req = 12345;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_EINVAL)
    {
        fail("unknown id kind: status");
    }
    if (req != 12345)
    {
        fail("unknown id kind: out changed");
    }
    j89_arena_destroy(&a);
}

static void test_string_id_null_str(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    jrpc89_status st;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_STRING;
    id.str = (const char *)0;
    id.len = 0;
    req = 12345;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_EINVAL)
    {
        fail("string id with NULL str: status");
    }
    if (req != 12345)
    {
        fail("string id with NULL str: out changed");
    }
    j89_arena_destroy(&a);
}

static void test_inexact_int_id(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    jrpc89_status st;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    req = 12345;
    id.num = 1.5;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_EINVAL)
    {
        fail("fractional id: status");
    }
    id.num = HUGE_VAL;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_EINVAL)
    {
        fail("infinite id: status");
    }
    id.num = 9007199254740994.0;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_EINVAL)
    {
        fail("id beyond 2^53: status");
    }
    id.num = 9007199254740992.0;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_OK)
    {
        fail("id at 2^53: status");
    }
    j89_arena_destroy(&a);
}

static void test_invalid_utf8_method(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    jrpc89_status st;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 7;
    req = 12345;
    st = jrpc89_request_new(&a, "\xff", 1, J89_BAD, &id, &req);
    if (st != JRPC89_EINVAL)
    {
        fail("invalid utf-8 method: status");
    }
    if (req != 12345)
    {
        fail("invalid utf-8 method: out changed");
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
    jrpc89_status st;
    j89_arena_init(&a);
    j89_string_new(&a, "\xff", 1);
    if (!j89_failed(&a))
    {
        fail("prefailed: setup did not fail the arena");
    }
    id.kind = JRPC89_ID_INT;
    id.num = 7;
    req = 12345;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_EINVAL)
    {
        fail("prefailed arena: status");
    }
    if (req != 12345)
    {
        fail("prefailed arena: out changed");
    }
    j89_arena_destroy(&a);
}

static void test_rpc_prefix_method(void)
{
    j89_arena a;
    j89_len req;
    j89_arena_init(&a);
    req = build_simple(&a, "rpc.discover", 11, J89_BAD);
    if (req == J89_BAD)
    {
        fail("rpc prefix method refused");
    }
    j89_arena_destroy(&a);
}

static void test_long_method(void)
{
    j89_arena a;
    j89_len req;
    char method[4001];
    size_t i;
    for (i = 0; i < 4000; i = i + 1)
    {
        method[i] = 'm';
    }
    method[4000] = '\0';
    j89_arena_init(&a);
    req = build_simple(&a, method, 4000, J89_BAD);
    if (req == J89_BAD)
    {
        fail("long method refused");
    }
    j89_arena_destroy(&a);
}

static void test_empty_string_id(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    jrpc89_status st;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_STRING;
    id.str = "";
    id.len = 0;
    st = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id, &req);
    if (st != JRPC89_OK)
    {
        fail("empty string id: status");
    }
    else
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"foo\",\"id\":\"\"}");
    }
    j89_arena_destroy(&a);
}

static void test_empty_array_params(void)
{
    j89_arena a;
    j89_len params;
    j89_len req;
    j89_arena_init(&a);
    params = j89_array_new(&a, 0);
    req = build_simple(&a, "foo", 3, params);
    if (req == J89_BAD)
    {
        fail("empty array params refused");
    }
    j89_arena_destroy(&a);
}

int main(void)
{
    test_int_id();
    test_string_id();
    test_null_id();
    test_notification();
    test_object_params();
    test_array_params();
    test_scalar_params();
    test_empty_method();
    test_null_method_zero_len();
    test_null_method_nonzero_len();
    test_embedded_nul_method();
    test_embedded_nul_string_id();
    test_null_arguments();
    test_illegal_id_kind();
    test_string_id_null_str();
    test_inexact_int_id();
    test_invalid_utf8_method();
    test_prefailed_arena();
    test_rpc_prefix_method();
    test_long_method();
    test_empty_string_id();
    test_empty_array_params();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_request: ok\n");
    return 0;
}
