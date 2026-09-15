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

static void test_int_id(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 7;
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id);
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
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id);
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
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id);
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
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id);
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
    req = jrpc89_request_new(&a, "foo", 3, params, &id);
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

static void test_array_params(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len params;
    j89_len req;
    j89_arena_init(&a);
    params = j89_array_new(&a, 1);
    j89_array_set(&a, params, 0, j89_integer_new(&a, 5));
    id.kind = JRPC89_ID_INT;
    id.num = 7;
    req = jrpc89_request_new(&a, "foo", 3, params, &id);
    if (!jrpc89_has_node(req))
    {
        fail("array params: request_new");
    }
    else
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
        j89_arena_init(&a);
        params = parse(texts[i], &a);
        if (!jrpc89_has_node(params))
        {
            fail("scalar params: parse");
        }
        else
        {
            id.kind = JRPC89_ID_INT;
            id.num = 7;
            req = jrpc89_request_new(&a, "foo", 3, params, &id);
            if (jrpc89_has_node(req))
            {
                fprintf(stderr, "FAIL: scalar params accepted: %s\n", texts[i]);
                failures = failures + 1;
            }
        }
        j89_arena_destroy(&a);
    }
}

static void test_empty_method(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 1;
    req = jrpc89_request_new(&a, "", 0, J89_BAD, &id);
    if (!jrpc89_has_node(req))
    {
        fail("empty method refused");
    }
    else
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"\",\"id\":1}");
    }
    j89_arena_destroy(&a);
}

static void test_null_method_zero_len(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 1;
    req = jrpc89_request_new(&a, (const char *)0, 0, J89_BAD, &id);
    if (!jrpc89_has_node(req))
    {
        fail("null method with zero length refused");
    }
    else
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"\",\"id\":1}");
    }
    j89_arena_destroy(&a);
}

static void test_null_method_nonzero_len(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 1;
    req = jrpc89_request_new(&a, (const char *)0, 3, J89_BAD, &id);
    if (jrpc89_has_node(req))
    {
        fail("null method with nonzero length accepted");
    }
    j89_arena_destroy(&a);
}

static void test_embedded_nul_method(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    static const char method[] = {'a', '\0', 'b'};
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 1;
    req = jrpc89_request_new(&a, method, 3, J89_BAD, &id);
    if (!jrpc89_has_node(req))
    {
        fail("embedded nul method refused");
    }
    else
    {
        expect_render(&a, req,
                      "{\"jsonrpc\":\"2.0\",\"method\":\"a\\u0000b\","
                      "\"id\":1}");
    }
    j89_arena_destroy(&a);
}

static void test_null_id_pointer(void)
{
    j89_arena a;
    j89_len req;
    j89_arena_init(&a);
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, (const jrpc89_id *)0);
    if (jrpc89_has_node(req))
    {
        fail("null id pointer accepted");
    }
    j89_arena_destroy(&a);
}

static void test_illegal_id_kind(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = (jrpc89_id_kind)99;
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id);
    if (jrpc89_has_node(req))
    {
        fail("unknown id kind accepted");
    }
    j89_arena_destroy(&a);
}

static void test_string_id_null_str(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_STRING;
    id.str = (const char *)0;
    id.len = 0;
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id);
    if (jrpc89_has_node(req))
    {
        fail("string id with NULL str accepted");
    }
    j89_arena_destroy(&a);
}

static void test_inexact_int_id(void)
{
    j89_arena a;
    jrpc89_id id;
    j89_len req;
    j89_arena_init(&a);
    id.kind = JRPC89_ID_INT;
    id.num = 1.5;
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id);
    if (jrpc89_has_node(req))
    {
        fail("fractional id accepted");
    }
    id.num = HUGE_VAL;
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id);
    if (jrpc89_has_node(req))
    {
        fail("infinite id accepted");
    }
    id.num = 9007199254740994.0;
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id);
    if (jrpc89_has_node(req))
    {
        fail("id beyond 2^53 accepted");
    }
    id.num = 9007199254740992.0;
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id);
    if (!jrpc89_has_node(req))
    {
        fail("id at 2^53 refused");
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
    req = jrpc89_request_new(&a, "\xff", 1, J89_BAD, &id);
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
    req = jrpc89_request_new(&a, "foo", 3, J89_BAD, &id);
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
    test_array_params();
    test_scalar_params();
    test_empty_method();
    test_null_method_zero_len();
    test_null_method_nonzero_len();
    test_embedded_nul_method();
    test_null_id_pointer();
    test_illegal_id_kind();
    test_string_id_null_str();
    test_inexact_int_id();
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
