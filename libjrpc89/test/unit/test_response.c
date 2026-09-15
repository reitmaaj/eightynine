/* test_response.c - unit tests for response parsing and validation. */
#include <stdio.h>
#include <string.h>

#include <jrpc89.h>

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static j89_len parse(const char *s, j89_arena *a)
{
    j89_len root;
    root = j89_parse(s, (j89_len)strlen(s), a);
    return root;
}

static void test_valid_result(void)
{
    j89_arena a;
    j89_len resp;
    j89_len result;
    int r;
    j89_arena_init(&a);
    resp = parse("{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":7}", &a);
    if (!jrpc89_has_node(resp))
    {
        fail("valid result: parse");
    }
    else
    {
        r = jrpc89_response_validate(&a, resp);
        if (r != 0)
        {
            fail("valid result: validate");
        }
        if (jrpc89_is_error(&a, resp))
        {
            fail("valid result: is_error true");
        }
        result = jrpc89_result_node(&a, resp);
        if (!jrpc89_has_node(result))
        {
            fail("valid result: result_node");
        }
    }
    j89_arena_destroy(&a);
}

static void test_valid_error(void)
{
    j89_arena a;
    j89_len resp;
    int r;
    j89_arena_init(&a);
    resp = parse("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32001,"
                 "\"message\":\"boom\"},\"id\":7}",
                 &a);
    if (!jrpc89_has_node(resp))
    {
        fail("valid error: parse");
    }
    else
    {
        r = jrpc89_response_validate(&a, resp);
        if (r != 0)
        {
            fail("valid error: validate");
        }
        if (!jrpc89_is_error(&a, resp))
        {
            fail("valid error: is_error false");
        }
    }
    j89_arena_destroy(&a);
}

static int expect_invalid(const char *label, const char *s)
{
    j89_arena a;
    j89_len resp;
    int r;
    j89_arena_init(&a);
    resp = parse(s, &a);
    if (!jrpc89_has_node(resp))
    {
        j89_arena_destroy(&a);
        return 1;
    }
    r = jrpc89_response_validate(&a, resp);
    j89_arena_destroy(&a);
    if (r == 0)
    {
        fail(label);
        return 0;
    }
    return 1;
}

static void test_rejections(void)
{
    expect_invalid("wrong version",
                   "{\"jsonrpc\":\"1.0\",\"result\":1,\"id\":7}");
    expect_invalid("both result and error",
                   "{\"jsonrpc\":\"2.0\",\"result\":1,\"error\":{\"code\":1},"
                   "\"id\":7}");
    expect_invalid("neither result nor error",
                   "{\"jsonrpc\":\"2.0\",\"id\":7}");
    expect_invalid("missing id", "{\"jsonrpc\":\"2.0\",\"result\":1}");
    expect_invalid("error not object",
                   "{\"jsonrpc\":\"2.0\",\"error\":123,\"id\":1}");
    expect_invalid(
        "error missing code",
        "{\"jsonrpc\":\"2.0\",\"error\":{\"message\":\"x\"},\"id\":1}");
    expect_invalid("error missing message",
                   "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1},\"id\":1}");
    expect_invalid("error code not int",
                   "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":\"x\","
                   "\"message\":\"y\"},\"id\":1}");
    expect_invalid("error message not string",
                   "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1,"
                   "\"message\":5},\"id\":1}");
}

static void test_illegal_id_kinds(void)
{
    expect_invalid("id boolean true",
                   "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":true}");
    expect_invalid("id boolean false",
                   "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":false}");
    expect_invalid("id float", "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":1.5}");
    expect_invalid("id array", "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":[]}");
    expect_invalid("id object", "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":{}}");
}

static void test_valid_null_id(void)
{
    j89_arena a;
    j89_len resp;
    jrpc89_id out;
    int r;
    j89_arena_init(&a);
    resp = parse("{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":null}", &a);
    if (!jrpc89_has_node(resp))
    {
        fail("null id: parse");
    }
    else
    {
        r = jrpc89_response_validate(&a, resp);
        if (r != 0)
        {
            fail("null id: validate");
        }
        r = jrpc89_id_of_response(&a, resp, &out);
        if (r != 0 || out.kind != JRPC89_ID_NULL)
        {
            fail("null id: extract");
        }
    }
    j89_arena_destroy(&a);
}

static void test_illegal_id_extract(void)
{
    j89_arena a;
    j89_len resp;
    jrpc89_id out;
    int r;
    j89_arena_init(&a);
    resp = parse("{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":true}", &a);
    if (!jrpc89_has_node(resp))
    {
        fail("illegal id extract: parse");
    }
    else
    {
        r = jrpc89_id_of_response(&a, resp, &out);
        if (r == 0)
        {
            fail("illegal id extract: accepted boolean id");
        }
    }
    j89_arena_destroy(&a);
}

static void test_id_of_response(void)
{
    j89_arena a;
    j89_len resp;
    jrpc89_id out;
    int r;
    j89_arena_init(&a);
    resp = parse("{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":\"abc\"}", &a);
    if (!jrpc89_has_node(resp))
    {
        fail("id_of_response: parse");
    }
    else
    {
        r = jrpc89_id_of_response(&a, resp, &out);
        if (r != 0)
        {
            fail("id_of_response: extract");
        }
        else if (out.kind != JRPC89_ID_STRING)
        {
            fail("id_of_response: kind");
        }
        else if (out.len != 3 || strncmp(out.str, "abc", 3) != 0)
        {
            fail("id_of_response: value");
        }
    }
    j89_arena_destroy(&a);
}

static void test_id_matches(void)
{
    jrpc89_id a;
    jrpc89_id b;
    jrpc89_id c;
    a.kind = JRPC89_ID_INT;
    a.num = 7;
    b.kind = JRPC89_ID_INT;
    b.num = 7;
    c.kind = JRPC89_ID_INT;
    c.num = 8;
    if (!jrpc89_id_matches(&a, &b))
    {
        fail("id_matches: equal ints");
    }
    if (jrpc89_id_matches(&a, &c))
    {
        fail("id_matches: different ints");
    }
    b.kind = JRPC89_ID_STRING;
    b.str = "abc";
    b.len = 3;
    if (jrpc89_id_matches(&a, &b))
    {
        fail("id_matches: int vs string");
    }
}

int main(void)
{
    test_valid_result();
    test_valid_error();
    test_rejections();
    test_illegal_id_kinds();
    test_valid_null_id();
    test_illegal_id_extract();
    test_id_of_response();
    test_id_matches();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_response: ok\n");
    return 0;
}
