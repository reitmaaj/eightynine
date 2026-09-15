/* test_error.c - unit tests for error codes and error-object access. */
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

static void test_is_reserved(void)
{
    if (!jrpc89_error_is_reserved(JRPC89_PARSE_ERROR))
    {
        fail("parse error not reserved");
    }
    if (!jrpc89_error_is_reserved(JRPC89_INVALID_REQUEST))
    {
        fail("invalid request not reserved");
    }
    if (!jrpc89_error_is_reserved(JRPC89_METHOD_NOT_FOUND))
    {
        fail("method not found not reserved");
    }
    if (!jrpc89_error_is_reserved(JRPC89_INVALID_PARAMS))
    {
        fail("invalid params not reserved");
    }
    if (!jrpc89_error_is_reserved(JRPC89_INTERNAL_ERROR))
    {
        fail("internal error not reserved");
    }
    if (!jrpc89_error_is_reserved(JRPC89_SERVER_ERROR_MIN))
    {
        fail("server error min not reserved");
    }
    if (!jrpc89_error_is_reserved(JRPC89_SERVER_ERROR_MAX))
    {
        fail("server error max not reserved");
    }
    if (jrpc89_error_is_reserved(-1))
    {
        fail("application code -1 reserved");
    }
    if (jrpc89_error_is_reserved(1234))
    {
        fail("application code 1234 reserved");
    }
}

static void test_reserved_gaps(void)
{
    if (jrpc89_error_is_reserved(-32769))
    {
        fail("code -32769 should not be reserved");
    }
    if (!jrpc89_error_is_reserved(-32768))
    {
        fail("code -32768 should be reserved");
    }
    if (!jrpc89_error_is_reserved(-32500))
    {
        fail("gap -32500 should be reserved");
    }
    if (!jrpc89_error_is_reserved(-32100))
    {
        fail("gap -32100 should be reserved");
    }
    if (!jrpc89_error_is_reserved(-32099))
    {
        fail("code -32099 should be reserved");
    }
    if (!jrpc89_error_is_reserved(-32000))
    {
        fail("code -32000 should be reserved");
    }
    if (jrpc89_error_is_reserved(-31999))
    {
        fail("code -31999 should not be reserved");
    }
}

static void test_accessors(void)
{
    j89_arena a;
    j89_len resp;
    j89_int code;
    const char *msg;
    j89_len msglen;
    j89_arena_init(&a);
    resp = parse("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32001,"
                 "\"message\":\"boom\",\"data\":{\"d\":1}},\"id\":7}",
                 &a);
    if (!jrpc89_has_node(resp))
    {
        fail("accessors: parse");
    }
    else
    {
        code = jrpc89_error_code(&a, resp);
        if (code != -32001)
        {
            fprintf(stderr, "FAIL: code got %.0f\n", code);
            failures = failures + 1;
        }
        msg = jrpc89_error_message(&a, resp);
        if (strcmp(msg, "boom") != 0)
        {
            fail("accessors: message");
        }
        msglen = jrpc89_error_message_length(&a, resp);
        if (msglen != 4)
        {
            fail("accessors: message length");
        }
        if (!jrpc89_has_node(jrpc89_error_data(&a, resp)))
        {
            fail("accessors: data present");
        }
    }
    j89_arena_destroy(&a);
}

static void test_absent_error(void)
{
    j89_arena a;
    j89_len resp;
    j89_arena_init(&a);
    resp = parse("{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":7}", &a);
    if (jrpc89_has_node(resp))
    {
        if (jrpc89_error_code(&a, resp) != 0)
        {
            fail("absent error: code nonzero");
        }
        if (strcmp(jrpc89_error_message(&a, resp), "") != 0)
        {
            fail("absent error: message not empty");
        }
        if (jrpc89_has_node(jrpc89_error_data(&a, resp)))
        {
            fail("absent error: data present");
        }
    }
    j89_arena_destroy(&a);
}

static void test_code_outside_int(void)
{
    j89_arena a;
    j89_len resp;
    j89_int code;
    j89_arena_init(&a);
    resp = parse("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":2147483648,"
                 "\"message\":\"big\"},\"id\":7}",
                 &a);
    if (!jrpc89_has_node(resp))
    {
        fail("outside int: parse");
    }
    else
    {
        code = jrpc89_error_code(&a, resp);
        if (code != 2147483648.0)
        {
            fprintf(stderr, "FAIL: outside int code got %.0f\n", code);
            failures = failures + 1;
        }
        if (jrpc89_error_is_reserved(code))
        {
            fail("outside int: positive code reserved");
        }
    }
    j89_arena_destroy(&a);
    j89_arena_init(&a);
    resp = parse("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-2147483649,"
                 "\"message\":\"big\"},\"id\":7}",
                 &a);
    if (!jrpc89_has_node(resp))
    {
        fail("outside int: negative parse");
    }
    else
    {
        code = jrpc89_error_code(&a, resp);
        if (code != -2147483649.0)
        {
            fprintf(stderr, "FAIL: negative outside int got %.0f\n", code);
            failures = failures + 1;
        }
    }
    j89_arena_destroy(&a);
}

int main(void)
{
    test_is_reserved();
    test_reserved_gaps();
    test_accessors();
    test_absent_error();
    test_code_outside_int();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_error: ok\n");
    return 0;
}
