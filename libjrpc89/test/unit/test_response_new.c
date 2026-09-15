/* test_response_new.c - unit tests for the response builders. */
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

static void set_id_int(jrpc89_id *id, j89_int v)
{
    id->kind = JRPC89_ID_INT;
    id->num = v;
    id->str = NULL;
    id->len = 0;
}

static void set_id_str(jrpc89_id *id, const char *s, j89_len n)
{
    id->kind = JRPC89_ID_STRING;
    id->num = 0;
    id->str = s;
    id->len = n;
}

static void set_id_null(jrpc89_id *id)
{
    id->kind = JRPC89_ID_NULL;
    id->num = 0;
    id->str = NULL;
    id->len = 0;
}

static void check_result(j89_arena *a, j89_len res, const char *expect,
                         const jrpc89_id *id)
{
    j89_len out;
    jrpc89_response r;
    jrpc89_status st;
    char buf[128];

    out = 12345;
    st = jrpc89_response_result_new(a, id, res, &out);
    if (st != JRPC89_OK)
    {
        fail("result build");
        return;
    }
    st = jrpc89_response_decode(a, out, &r);
    if (st != JRPC89_OK)
    {
        fail("result decode");
        return;
    }
    if (r.kind != JRPC89_RESPONSE_RESULT)
    {
        fail("result kind");
    }
    if (!jrpc89_id_equal(&r.id, id))
    {
        fail("result id");
    }
    if (r.result == J89_BAD)
    {
        fail("result node");
        return;
    }
    if (render_to(a, r.result, buf, sizeof buf) != 0)
    {
        fail("result render");
        return;
    }
    if (strcmp(buf, expect) != 0)
    {
        fprintf(stderr, "FAIL: result <%s> want <%s>\n", buf, expect);
        failures = failures + 1;
    }
}

static void check_error(j89_arena *a, const jrpc89_id *id, j89_int code,
                        const char *msg, j89_len msg_len, j89_len data,
                        const char *expect_data)
{
    j89_len out;
    jrpc89_response r;
    jrpc89_status st;
    char buf[128];

    out = 12345;
    st = jrpc89_response_error_new(a, id, code, msg, msg_len, data, &out);
    if (st != JRPC89_OK)
    {
        fail("error build");
        return;
    }
    st = jrpc89_response_decode(a, out, &r);
    if (st != JRPC89_OK)
    {
        fail("error decode");
        return;
    }
    if (r.kind != JRPC89_RESPONSE_ERROR)
    {
        fail("error kind");
    }
    if (r.error.code != code)
    {
        fail("error code");
    }
    if (r.error.message_len != msg_len)
    {
        fail("error message length");
    }
    if (msg_len > 0)
    {
        if (memcmp(r.error.message, msg, msg_len) != 0)
        {
            fail("error message bytes");
        }
    }
    if (!jrpc89_id_equal(&r.id, id))
    {
        fail("error id");
    }
    if (expect_data == NULL)
    {
        if (r.error.data != J89_BAD)
        {
            fail("error data present");
        }
        return;
    }
    if (r.error.data == J89_BAD)
    {
        fail("error data absent");
        return;
    }
    if (render_to(a, r.error.data, buf, sizeof buf) != 0)
    {
        fail("error data render");
        return;
    }
    if (strcmp(buf, expect_data) != 0)
    {
        fprintf(stderr, "FAIL: error data <%s> want <%s>\n", buf, expect_data);
        failures = failures + 1;
    }
}

static void test_result_ids(void)
{
    j89_arena a;
    j89_len res;
    jrpc89_id id;

    j89_arena_init(&a);
    res = j89_integer_new(&a, 42);
    set_id_int(&id, 7);
    check_result(&a, res, "42", &id);
    set_id_str(&id, "abc", 3);
    check_result(&a, res, "42", &id);
    set_id_null(&id);
    check_result(&a, res, "42", &id);
    j89_arena_destroy(&a);
}

static void test_result_values(void)
{
    j89_arena a;
    j89_len arr;
    j89_len obj;
    jrpc89_id id;

    j89_arena_init(&a);
    set_id_int(&id, 1);
    check_result(&a, j89_null_new(&a), "null", &id);
    check_result(&a, j89_bool_new(&a, 1), "true", &id);
    check_result(&a, j89_bool_new(&a, 0), "false", &id);
    check_result(&a, j89_integer_new(&a, -5), "-5", &id);
    check_result(&a, j89_string_new(&a, "ok", 2), "\"ok\"", &id);
    arr = j89_array_new(&a, 2);
    j89_array_set(&a, arr, 0, j89_integer_new(&a, 1));
    j89_array_set(&a, arr, 1, j89_integer_new(&a, 2));
    check_result(&a, arr, "[1,2]", &id);
    obj = j89_object_new(&a, 1);
    j89_object_set(&a, obj, 0, "a", 1, j89_integer_new(&a, 1));
    check_result(&a, obj, "{\"a\":1}", &id);
    j89_arena_destroy(&a);
}

static void test_error_basic(void)
{
    j89_arena a;
    jrpc89_id id;

    j89_arena_init(&a);
    set_id_int(&id, 1);
    check_error(&a, &id, -32601, "method not found", 16, J89_BAD, NULL);
    set_id_str(&id, "s", 1);
    check_error(&a, &id, -32700, "parse", 5, J89_BAD, NULL);
    set_id_null(&id);
    check_error(&a, &id, 123, "app", 3, J89_BAD, NULL);
    j89_arena_destroy(&a);
}

static void test_error_data(void)
{
    j89_arena a;
    j89_len arr;
    j89_len obj;
    jrpc89_id id;

    j89_arena_init(&a);
    set_id_int(&id, 1);
    check_error(&a, &id, -32000, "e", 1, j89_null_new(&a), "null");
    check_error(&a, &id, -32000, "e", 1, j89_bool_new(&a, 1), "true");
    check_error(&a, &id, -32000, "e", 1, j89_integer_new(&a, 9), "9");
    check_error(&a, &id, -32000, "e", 1, j89_string_new(&a, "d", 1), "\"d\"");
    arr = j89_array_new(&a, 1);
    j89_array_set(&a, arr, 0, j89_integer_new(&a, 1));
    check_error(&a, &id, -32000, "e", 1, arr, "[1]");
    obj = j89_object_new(&a, 1);
    j89_object_set(&a, obj, 0, "k", 1, j89_integer_new(&a, 2));
    check_error(&a, &id, -32000, "e", 1, obj, "{\"k\":2}");
    j89_arena_destroy(&a);
}

static void test_error_messages(void)
{
    j89_arena a;
    jrpc89_id id;
    const char *nul_msg;

    j89_arena_init(&a);
    set_id_int(&id, 1);
    nul_msg = "a\0b";
    check_error(&a, &id, 1, NULL, 0, J89_BAD, NULL);
    check_error(&a, &id, 1, "", 0, J89_BAD, NULL);
    check_error(&a, &id, 1, nul_msg, 3, J89_BAD, NULL);
    check_error(&a, &id, 1, "h\xc3\xa9", 3, J89_BAD, NULL);
    j89_arena_destroy(&a);
}

static void test_error_codes(void)
{
    j89_arena a;
    jrpc89_id id;

    j89_arena_init(&a);
    set_id_int(&id, 1);
    check_error(&a, &id, -32700, "m", 1, J89_BAD, NULL);
    check_error(&a, &id, -32601, "m", 1, J89_BAD, NULL);
    check_error(&a, &id, -32603, "m", 1, J89_BAD, NULL);
    check_error(&a, &id, -32768, "m", 1, J89_BAD, NULL);
    check_error(&a, &id, -32000, "m", 1, J89_BAD, NULL);
    check_error(&a, &id, 0, "m", 1, J89_BAD, NULL);
    check_error(&a, &id, 123, "m", 1, J89_BAD, NULL);
    check_error(&a, &id, -123, "m", 1, J89_BAD, NULL);
    check_error(&a, &id, 32768, "m", 1, J89_BAD, NULL);
    check_error(&a, &id, 9007199254740992.0, "m", 1, J89_BAD, NULL);
    check_error(&a, &id, -9007199254740992.0, "m", 1, J89_BAD, NULL);
    j89_arena_destroy(&a);
}

static void test_none_id(void)
{
    j89_arena a;
    j89_len res;
    j89_len out;
    jrpc89_id id;
    jrpc89_status st;

    j89_arena_init(&a);
    res = j89_integer_new(&a, 1);
    set_id_int(&id, 1);
    id.kind = JRPC89_ID_NONE;
    out = 12345;
    st = jrpc89_response_result_new(&a, &id, res, &out);
    if (st != JRPC89_EINVAL)
    {
        fail("NONE id result");
    }
    if (out != 12345)
    {
        fail("NONE id result out");
    }
    out = 12345;
    st = jrpc89_response_error_new(&a, &id, 1, "m", 1, J89_BAD, &out);
    if (st != JRPC89_EINVAL)
    {
        fail("NONE id error");
    }
    if (out != 12345)
    {
        fail("NONE id error out");
    }
    j89_arena_destroy(&a);
}

static void test_bad_arguments(void)
{
    j89_arena a;
    j89_len res;
    j89_len out;
    jrpc89_id id;
    jrpc89_status st;

    j89_arena_init(&a);
    res = j89_integer_new(&a, 1);
    set_id_int(&id, 1);

    out = 12345;
    st = jrpc89_response_result_new(NULL, &id, res, &out);
    if (st != JRPC89_EINVAL)
    {
        fail("NULL arena");
    }
    st = jrpc89_response_result_new(&a, NULL, res, &out);
    if (st != JRPC89_EINVAL)
    {
        fail("NULL id");
    }
    st = jrpc89_response_result_new(&a, &id, res, NULL);
    if (st != JRPC89_EINVAL)
    {
        fail("NULL out");
    }
    st = jrpc89_response_result_new(&a, &id, J89_BAD, &out);
    if (st != JRPC89_EINVAL)
    {
        fail("J89_BAD result");
    }
    if (out != 12345)
    {
        fail("bad result out");
    }

    out = 12345;
    st = jrpc89_response_error_new(&a, &id, 1, NULL, 5, J89_BAD, &out);
    if (st != JRPC89_EINVAL)
    {
        fail("NULL message with length");
    }
    st = jrpc89_response_error_new(&a, &id, 1.5, "m", 1, J89_BAD, &out);
    if (st != JRPC89_EINVAL)
    {
        fail("fractional code");
    }
    st = jrpc89_response_error_new(&a, &id, 1.0e300, "m", 1, J89_BAD, &out);
    if (st != JRPC89_EINVAL)
    {
        fail("huge code");
    }
    st = jrpc89_response_error_new(&a, &id, 1, "m", 1, J89_BAD, NULL);
    if (st != JRPC89_EINVAL)
    {
        fail("NULL out error");
    }
    if (out != 12345)
    {
        fail("bad error out");
    }
    j89_arena_destroy(&a);
}

static void test_invalid_utf8(void)
{
    j89_arena a;
    j89_len out;
    jrpc89_id id;
    jrpc89_status st;

    j89_arena_init(&a);
    set_id_int(&id, 1);
    out = 12345;
    st = jrpc89_response_error_new(&a, &id, 1, "\xff", 1, J89_BAD, &out);
    if (st != JRPC89_EINVAL)
    {
        fail("invalid utf8 message");
    }
    if (out != 12345)
    {
        fail("invalid utf8 out");
    }
    if (!j89_failed(&a))
    {
        fail("invalid utf8 arena");
    }
    j89_arena_destroy(&a);
}

static void test_dirty_arena(void)
{
    j89_arena a;
    j89_len res;
    j89_len out;
    j89_len bad;
    jrpc89_id id;
    jrpc89_status st;

    j89_arena_init(&a);
    res = j89_integer_new(&a, 1);
    set_id_int(&id, 1);
    bad = j89_string_new(&a, "\xff", 1);
    if (bad != J89_BAD)
    {
        fail("dirty setup");
    }
    out = 12345;
    st = jrpc89_response_result_new(&a, &id, res, &out);
    if (st != JRPC89_EINVAL)
    {
        fail("dirty arena");
    }
    if (out != 12345)
    {
        fail("dirty arena out");
    }
    j89_arena_destroy(&a);
}

static void test_round_trip(void)
{
    j89_arena a;
    j89_arena b;
    j89_len res;
    j89_len out;
    j89_len node;
    jrpc89_id id;
    jrpc89_response r;
    jrpc89_status st;
    char text[256];
    char ra[64];
    char rb[64];
    int rok;

    j89_arena_init(&a);
    set_id_int(&id, 7);
    res = j89_string_new(&a, "value", 5);
    out = 12345;
    st = jrpc89_response_result_new(&a, &id, res, &out);
    if (st != JRPC89_OK)
    {
        fail("round trip result build");
        j89_arena_destroy(&a);
        return;
    }
    rok = render_to(&a, out, text, sizeof text);
    if (rok != 0)
    {
        fail("round trip result render");
        j89_arena_destroy(&a);
        return;
    }
    j89_arena_init(&b);
    node = parse(text, &b);
    if (node == J89_BAD)
    {
        fail("round trip result parse");
        j89_arena_destroy(&b);
        j89_arena_destroy(&a);
        return;
    }
    st = jrpc89_response_decode(&b, node, &r);
    if (st != JRPC89_OK)
    {
        fail("round trip result decode");
    }
    if (r.kind != JRPC89_RESPONSE_RESULT)
    {
        fail("round trip result kind");
    }
    if (!jrpc89_id_equal(&r.id, &id))
    {
        fail("round trip result id");
    }
    rok = render_to(&a, res, ra, sizeof ra);
    if (rok != 0)
    {
        fail("round trip result render a");
    }
    rok = render_to(&b, r.result, rb, sizeof rb);
    if (rok != 0)
    {
        fail("round trip result render b");
    }
    if (strcmp(ra, rb) != 0)
    {
        fail("round trip result bytes");
    }
    j89_arena_destroy(&b);
    j89_arena_destroy(&a);

    j89_arena_init(&a);
    set_id_str(&id, "x", 1);
    out = 12345;
    st = jrpc89_response_error_new(&a, &id, -32602, "bad params", 10,
                                   j89_integer_new(&a, 3), &out);
    if (st != JRPC89_OK)
    {
        fail("round trip error build");
        j89_arena_destroy(&a);
        return;
    }
    rok = render_to(&a, out, text, sizeof text);
    if (rok != 0)
    {
        fail("round trip error render");
        j89_arena_destroy(&a);
        return;
    }
    j89_arena_init(&b);
    node = parse(text, &b);
    if (node == J89_BAD)
    {
        fail("round trip error parse");
        j89_arena_destroy(&b);
        j89_arena_destroy(&a);
        return;
    }
    st = jrpc89_response_decode(&b, node, &r);
    if (st != JRPC89_OK)
    {
        fail("round trip error decode");
    }
    if (r.kind != JRPC89_RESPONSE_ERROR)
    {
        fail("round trip error kind");
    }
    if (r.error.code != -32602)
    {
        fail("round trip error code");
    }
    if (r.error.message_len != 10)
    {
        fail("round trip error message length");
    }
    if (memcmp(r.error.message, "bad params", 10) != 0)
    {
        fail("round trip error message bytes");
    }
    if (!jrpc89_id_equal(&r.id, &id))
    {
        fail("round trip error id");
    }
    rok = render_to(&b, r.error.data, rb, sizeof rb);
    if (rok != 0)
    {
        fail("round trip error data render");
    }
    if (strcmp(rb, "3") != 0)
    {
        fail("round trip error data bytes");
    }
    j89_arena_destroy(&b);
    j89_arena_destroy(&a);
}

int main(void)
{
    test_result_ids();
    test_result_values();
    test_error_basic();
    test_error_data();
    test_error_messages();
    test_error_codes();
    test_none_id();
    test_bad_arguments();
    test_invalid_utf8();
    test_dirty_arena();
    test_round_trip();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_response_new: ok\n");
    return 0;
}
