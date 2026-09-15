/* test_response.c - unit tests for jrpc89_response_decode. */
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

static jrpc89_status decode_text(const char *s, j89_arena *a,
                                 jrpc89_response *out)
{
    j89_len resp;
    jrpc89_status st;
    resp = parse(s, a);
    if (resp == J89_BAD)
    {
        return JRPC89_EPROTO;
    }
    st = jrpc89_response_decode(a, resp, out);
    return st;
}

static void fill_sentinel(jrpc89_response *out)
{
    out->kind = (jrpc89_response_kind)99;
    out->id.kind = (jrpc89_id_kind)98;
    out->id.num = 1;
    out->id.str = "sentinel";
    out->id.len = 8;
    out->result = 4242;
    out->error.code = 17;
    out->error.message = "sentinel";
    out->error.message_len = 8;
    out->error.data = 4243;
}

static int sentinel_intact(const jrpc89_response *out)
{
    if (out->kind != (jrpc89_response_kind)99)
    {
        return 0;
    }
    if (out->id.kind != (jrpc89_id_kind)98)
    {
        return 0;
    }
    if (out->result != 4242)
    {
        return 0;
    }
    if (out->error.code != 17)
    {
        return 0;
    }
    if (out->error.data != 4243)
    {
        return 0;
    }
    return 1;
}

static void test_result_int_id(void)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    st = decode_text("{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":7}", &a, &out);
    if (st != JRPC89_OK)
    {
        fail("result int id: status");
    }
    else
    {
        if (out.kind != JRPC89_RESPONSE_RESULT)
        {
            fail("result int id: kind");
        }
        if (out.result == J89_BAD)
        {
            fail("result int id: result missing");
        }
        if (out.id.kind != JRPC89_ID_INT || out.id.num != 7)
        {
            fail("result int id: id");
        }
        if (out.error.data != J89_BAD || out.error.message_len != 0)
        {
            fail("result int id: error not zeroed");
        }
    }
    j89_arena_destroy(&a);
}

static void test_result_string_id(void)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    st = decode_text("{\"jsonrpc\":\"2.0\",\"result\":\"x\",\"id\":\"abc\"}",
                     &a, &out);
    if (st != JRPC89_OK)
    {
        fail("result string id: status");
    }
    else if (out.id.kind != JRPC89_ID_STRING)
    {
        fail("result string id: kind");
    }
    else if (out.id.len != 3 || strncmp(out.id.str, "abc", 3) != 0)
    {
        fail("result string id: bytes");
    }
    j89_arena_destroy(&a);
}

static void test_result_null_id(void)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    st =
        decode_text("{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":null}", &a, &out);
    if (st != JRPC89_OK)
    {
        fail("result null id: status");
    }
    else if (out.id.kind != JRPC89_ID_NULL)
    {
        fail("result null id: kind");
    }
    j89_arena_destroy(&a);
}

static void test_result_null_value(void)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    st =
        decode_text("{\"jsonrpc\":\"2.0\",\"result\":null,\"id\":1}", &a, &out);
    if (st != JRPC89_OK)
    {
        fail("result null value: status");
    }
    else if (out.result == J89_BAD)
    {
        fail("result null value: result missing");
    }
    j89_arena_destroy(&a);
}

static void test_error_with_data(void)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    st = decode_text("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32001,"
                     "\"message\":\"boom\",\"data\":{\"d\":1}},\"id\":7}",
                     &a, &out);
    if (st != JRPC89_OK)
    {
        fail("error with data: status");
    }
    else
    {
        if (out.kind != JRPC89_RESPONSE_ERROR)
        {
            fail("error with data: kind");
        }
        if (out.result != J89_BAD)
        {
            fail("error with data: result not BAD");
        }
        if (out.error.code != -32001)
        {
            fail("error with data: code");
        }
        if (out.error.message_len != 4)
        {
            fail("error with data: message length");
        }
        if (strncmp(out.error.message, "boom", 4) != 0)
        {
            fail("error with data: message bytes");
        }
        if (out.error.data == J89_BAD)
        {
            fail("error with data: data missing");
        }
    }
    j89_arena_destroy(&a);
}

static void test_error_without_data(void)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    st = decode_text("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1,"
                     "\"message\":\"x\"},\"id\":7}",
                     &a, &out);
    if (st != JRPC89_OK)
    {
        fail("error without data: status");
    }
    else if (out.error.data != J89_BAD)
    {
        fail("error without data: data present");
    }
    j89_arena_destroy(&a);
}

static void test_error_data_null(void)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    st = decode_text("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1,"
                     "\"message\":\"x\",\"data\":null},\"id\":7}",
                     &a, &out);
    if (st != JRPC89_OK)
    {
        fail("error data null: status");
    }
    else if (out.error.data == J89_BAD)
    {
        fail("error data null: data absent");
    }
    j89_arena_destroy(&a);
}

static void test_embedded_nul_strings(void)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    st = decode_text("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1,"
                     "\"message\":\"a\\u0000b\"},\"id\":\"x\\u0000y\"}",
                     &a, &out);
    if (st != JRPC89_OK)
    {
        fail("embedded nul: status");
    }
    else
    {
        if (out.error.message_len != 3)
        {
            fail("embedded nul: message length");
        }
        if (out.error.message[1] != '\0')
        {
            fail("embedded nul: message byte");
        }
        if (out.id.kind != JRPC89_ID_STRING || out.id.len != 3)
        {
            fail("embedded nul: id");
        }
        if (out.id.str[1] != '\0')
        {
            fail("embedded nul: id byte");
        }
    }
    j89_arena_destroy(&a);
}

static int expect_invalid(const char *label, const char *s)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    fill_sentinel(&out);
    st = decode_text(s, &a, &out);
    j89_arena_destroy(&a);
    if (st == JRPC89_OK)
    {
        fail(label);
        return 0;
    }
    if (!sentinel_intact(&out))
    {
        fail(label);
        return 0;
    }
    return 1;
}

static void test_rejections(void)
{
    expect_invalid("root array", "[]");
    expect_invalid("root string", "\"x\"");
    expect_invalid("root number", "123");
    expect_invalid("root null", "null");
    expect_invalid("wrong version",
                   "{\"jsonrpc\":\"1.0\",\"result\":1,\"id\":7}");
    expect_invalid("version not string",
                   "{\"jsonrpc\":2.0,\"result\":1,\"id\":7}");
    expect_invalid("missing version", "{\"result\":1,\"id\":7}");
    expect_invalid("both result and error",
                   "{\"jsonrpc\":\"2.0\",\"result\":1,\"error\":{\"code\":1},"
                   "\"id\":7}");
    expect_invalid("neither result nor error",
                   "{\"jsonrpc\":\"2.0\",\"id\":7}");
    expect_invalid("error not object",
                   "{\"jsonrpc\":\"2.0\",\"error\":123,\"id\":1}");
    expect_invalid("error null",
                   "{\"jsonrpc\":\"2.0\",\"error\":null,\"id\":1}");
    expect_invalid(
        "error missing code",
        "{\"jsonrpc\":\"2.0\",\"error\":{\"message\":\"x\"},\"id\":1}");
    expect_invalid("error missing message",
                   "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1},\"id\":1}");
    expect_invalid("error code not int",
                   "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":\"x\","
                   "\"message\":\"y\"},\"id\":1}");
    expect_invalid("error code float",
                   "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1.5,"
                   "\"message\":\"y\"},\"id\":1}");
    expect_invalid("error code bool",
                   "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":true,"
                   "\"message\":\"y\"},\"id\":1}");
    expect_invalid("error message not string",
                   "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1,"
                   "\"message\":5},\"id\":1}");
    expect_invalid("missing id", "{\"jsonrpc\":\"2.0\",\"result\":1}");
    expect_invalid("id boolean true",
                   "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":true}");
    expect_invalid("id boolean false",
                   "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":false}");
    expect_invalid("id float", "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":1.5}");
    expect_invalid("id array", "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":[]}");
    expect_invalid("id object", "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":{}}");
}

static void test_bad_node(void)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    fill_sentinel(&out);
    st = jrpc89_response_decode(&a, J89_BAD, &out);
    if (st != JRPC89_EPROTO)
    {
        fail("bad node: status");
    }
    if (!sentinel_intact(&out))
    {
        fail("bad node: out changed");
    }
    j89_arena_destroy(&a);
}

static void test_null_arguments(void)
{
    j89_arena a;
    jrpc89_response out;
    j89_len resp;
    jrpc89_status st;
    j89_arena_init(&a);
    resp = parse("{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":7}", &a);
    if (resp == J89_BAD)
    {
        fail("null arguments: parse");
    }
    else
    {
        fill_sentinel(&out);
        st = jrpc89_response_decode((j89_arena *)0, resp, &out);
        if (st != JRPC89_EINVAL)
        {
            fail("null arena: status");
        }
        st = jrpc89_response_decode(&a, resp, (jrpc89_response *)0);
        if (st != JRPC89_EINVAL)
        {
            fail("null out: status");
        }
        if (!sentinel_intact(&out))
        {
            fail("null arguments: out changed");
        }
    }
    j89_arena_destroy(&a);
}

static void test_dirty_arena(void)
{
    j89_arena a;
    jrpc89_response out;
    j89_len resp;
    jrpc89_status st;
    j89_arena_init(&a);
    resp = parse("{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":7}", &a);
    if (resp == J89_BAD)
    {
        fail("dirty arena: parse");
    }
    else
    {
        j89_string_new(&a, "\xff", 1);
        fill_sentinel(&out);
        st = jrpc89_response_decode(&a, resp, &out);
        if (st != JRPC89_EINVAL)
        {
            fail("dirty arena: status");
        }
        if (!sentinel_intact(&out))
        {
            fail("dirty arena: out changed");
        }
    }
    j89_arena_destroy(&a);
}

static void test_code_outside_int(void)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    st = decode_text("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":2147483648,"
                     "\"message\":\"big\"},\"id\":7}",
                     &a, &out);
    if (st != JRPC89_OK)
    {
        fail("code outside int: status");
    }
    else if (out.error.code != 2147483648.0)
    {
        fprintf(stderr, "FAIL: code outside int got %.0f\n", out.error.code);
        failures = failures + 1;
    }
    j89_arena_destroy(&a);
}

static void test_id_boundaries(void)
{
    static const char *texts[] = {
        "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":0}",
        "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":-1}",
        "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":9007199254740992}",
        "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":-9007199254740992}",
        "{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":\"\"}"};
    static const j89_int values[] = {0, -1, 9007199254740992.0,
                                     -9007199254740992.0, 0};
    static const int kinds[] = {JRPC89_ID_INT, JRPC89_ID_INT, JRPC89_ID_INT,
                                JRPC89_ID_INT, JRPC89_ID_STRING};
    size_t i;
    for (i = 0; i < sizeof(texts) / sizeof(texts[0]); i = i + 1)
    {
        j89_arena a;
        jrpc89_response out;
        jrpc89_status st;
        j89_arena_init(&a);
        st = decode_text(texts[i], &a, &out);
        if (st != JRPC89_OK)
        {
            fail("id boundary: status");
        }
        else
        {
            if (out.id.kind != (jrpc89_id_kind)kinds[i])
            {
                fail("id boundary: kind");
            }
            if (out.id.kind == JRPC89_ID_INT)
            {
                if (out.id.num != values[i])
                {
                    fprintf(stderr, "FAIL: id boundary value %.0f\n",
                            out.id.num);
                    failures = failures + 1;
                }
            }
            else if (out.id.len != 0)
            {
                fail("id boundary: empty string length");
            }
        }
        j89_arena_destroy(&a);
    }
}

static void test_error_code_boundaries(void)
{
    static const char *texts[] = {
        "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32768,"
        "\"message\":\"m\"},\"id\":1}",
        "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32000,"
        "\"message\":\"m\"},\"id\":1}",
        "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":32768,"
        "\"message\":\"m\"},\"id\":1}"};
    static const j89_int values[] = {-32768, -32000, 32768};
    size_t i;
    for (i = 0; i < sizeof(texts) / sizeof(texts[0]); i = i + 1)
    {
        j89_arena a;
        jrpc89_response out;
        jrpc89_status st;
        j89_arena_init(&a);
        st = decode_text(texts[i], &a, &out);
        if (st != JRPC89_OK)
        {
            fail("code boundary: status");
        }
        else if (out.error.code != values[i])
        {
            fprintf(stderr, "FAIL: code boundary value %.0f\n", out.error.code);
            failures = failures + 1;
        }
        j89_arena_destroy(&a);
    }
}

static void test_extra_members_ignored(void)
{
    j89_arena a;
    jrpc89_response out;
    jrpc89_status st;
    j89_arena_init(&a);
    st = decode_text("{\"jsonrpc\":\"2.0\",\"result\":false,"
                     "\"id\":1,\"meta\":{\"x\":1}}",
                     &a, &out);
    if (st != JRPC89_OK)
    {
        fail("extra members: status");
    }
    else if (out.result == J89_BAD)
    {
        fail("extra members: result");
    }
    j89_arena_destroy(&a);
}

int main(void)
{
    test_result_int_id();
    test_result_string_id();
    test_result_null_id();
    test_result_null_value();
    test_error_with_data();
    test_error_without_data();
    test_error_data_null();
    test_embedded_nul_strings();
    test_rejections();
    test_bad_node();
    test_null_arguments();
    test_dirty_arena();
    test_code_outside_int();
    test_id_boundaries();
    test_error_code_boundaries();
    test_extra_members_ignored();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_response: ok\n");
    return 0;
}
