/* test_request_decode.c - unit tests for jrpc89_request_decode. */
#include <stdio.h>
#include <string.h>

#include <jrpc89.h>

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static const char sentinel_byte = 'S';

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

static void set_sentinel(jrpc89_request *r)
{
    r->method = &sentinel_byte;
    r->method_len = 99;
    r->params = (j89_len)77;
    r->id.kind = JRPC89_ID_STRING;
    r->id.num = 0;
    r->id.str = &sentinel_byte;
    r->id.len = 55;
}

static int is_sentinel(const jrpc89_request *r)
{
    int ok;

    ok = 1;
    if (r->method != &sentinel_byte)
    {
        ok = 0;
    }
    if (r->method_len != 99)
    {
        ok = 0;
    }
    if (r->params != (j89_len)77)
    {
        ok = 0;
    }
    if (r->id.kind != JRPC89_ID_STRING)
    {
        ok = 0;
    }
    if (r->id.str != &sentinel_byte)
    {
        ok = 0;
    }
    if (r->id.len != 55)
    {
        ok = 0;
    }
    return ok;
}

static void expect_bad(const char *json, jrpc89_status want)
{
    j89_arena a;
    j89_len node;
    jrpc89_request r;
    jrpc89_status st;

    j89_arena_init(&a);
    node = parse(json, &a);
    if (node == J89_BAD)
    {
        fprintf(stderr, "FAIL: parse <%s>\n", json);
        failures = failures + 1;
        j89_arena_destroy(&a);
        return;
    }
    set_sentinel(&r);
    st = jrpc89_request_decode(&a, node, &r);
    if (st != want)
    {
        fprintf(stderr, "FAIL: <%s> status %d want %d\n", json, (int)st,
                (int)want);
        failures = failures + 1;
    }
    if (!is_sentinel(&r))
    {
        fprintf(stderr, "FAIL: <%s> modified out\n", json);
        failures = failures + 1;
    }
    j89_arena_destroy(&a);
}

static void test_minimal(void)
{
    j89_arena a;
    j89_len node;
    jrpc89_request r;
    jrpc89_status st;

    j89_arena_init(&a);
    node = parse("{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}", &a);
    if (node == J89_BAD)
    {
        fail("parse minimal");
        j89_arena_destroy(&a);
        return;
    }
    set_sentinel(&r);
    st = jrpc89_request_decode(&a, node, &r);
    if (st != JRPC89_OK)
    {
        fail("decode minimal");
        j89_arena_destroy(&a);
        return;
    }
    if (r.method_len != 4)
    {
        fail("method_len");
    }
    if (memcmp(r.method, "ping", 4) != 0)
    {
        fail("method bytes");
    }
    if (r.params != J89_BAD)
    {
        fail("params not absent");
    }
    if (r.id.kind != JRPC89_ID_INT)
    {
        fail("id kind");
    }
    if (r.id.num != 1.0)
    {
        fail("id value");
    }
    j89_arena_destroy(&a);
}

static void test_notification(void)
{
    j89_arena a;
    j89_len node;
    jrpc89_request r;
    jrpc89_status st;

    j89_arena_init(&a);
    node = parse("{\"jsonrpc\":\"2.0\",\"method\":\"tick\"}", &a);
    if (node == J89_BAD)
    {
        fail("parse notification");
        j89_arena_destroy(&a);
        return;
    }
    set_sentinel(&r);
    st = jrpc89_request_decode(&a, node, &r);
    if (st != JRPC89_OK)
    {
        fail("decode notification");
        j89_arena_destroy(&a);
        return;
    }
    if (r.id.kind != JRPC89_ID_NONE)
    {
        fail("notification id");
    }
    if (r.params != J89_BAD)
    {
        fail("notification params");
    }
    j89_arena_destroy(&a);
}

static void expect_params(const char *json)
{
    j89_arena a;
    j89_len node;
    jrpc89_request r;
    jrpc89_status st;

    j89_arena_init(&a);
    node = parse(json, &a);
    if (node == J89_BAD)
    {
        fail("parse params");
        j89_arena_destroy(&a);
        return;
    }
    set_sentinel(&r);
    st = jrpc89_request_decode(&a, node, &r);
    if (st != JRPC89_OK)
    {
        fail("decode params");
    }
    if (r.params == J89_BAD)
    {
        fail("params absent");
    }
    j89_arena_destroy(&a);
}

static void test_params_forms(void)
{
    expect_params("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"params\":[]}");
    expect_params("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"params\":[1,2]}");
    expect_params("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"params\":{}}");
    expect_params(
        "{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"params\":{\"a\":1}}");
}

static void expect_id(const char *json, jrpc89_id_kind kind)
{
    j89_arena a;
    j89_len node;
    jrpc89_request r;
    jrpc89_status st;

    j89_arena_init(&a);
    node = parse(json, &a);
    if (node == J89_BAD)
    {
        fail("parse id");
        j89_arena_destroy(&a);
        return;
    }
    set_sentinel(&r);
    st = jrpc89_request_decode(&a, node, &r);
    if (st != JRPC89_OK)
    {
        fail("decode id");
    }
    if (r.id.kind != kind)
    {
        fail("id kind");
    }
    j89_arena_destroy(&a);
}

static void test_id_forms(void)
{
    expect_id("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":7}", JRPC89_ID_INT);
    expect_id("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":\"abc\"}",
              JRPC89_ID_STRING);
    expect_id("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":null}",
              JRPC89_ID_NULL);
}

static void test_method_strings(void)
{
    j89_arena a;
    j89_len node;
    jrpc89_request r;
    jrpc89_status st;

    j89_arena_init(&a);
    node = parse("{\"jsonrpc\":\"2.0\",\"method\":\"a\\u0000b\"}", &a);
    if (node == J89_BAD)
    {
        fail("parse nul method");
        j89_arena_destroy(&a);
        return;
    }
    set_sentinel(&r);
    st = jrpc89_request_decode(&a, node, &r);
    if (st != JRPC89_OK)
    {
        fail("decode nul method");
    }
    if (r.method_len != 3)
    {
        fail("nul method len");
    }
    if (r.method[0] != 'a')
    {
        fail("nul method byte 0");
    }
    if (r.method[1] != '\0')
    {
        fail("nul method byte 1");
    }
    if (r.method[2] != 'b')
    {
        fail("nul method byte 2");
    }
    j89_arena_destroy(&a);

    j89_arena_init(&a);
    node = parse("{\"jsonrpc\":\"2.0\",\"method\":\"\"}", &a);
    if (node == J89_BAD)
    {
        fail("parse empty method");
        j89_arena_destroy(&a);
        return;
    }
    set_sentinel(&r);
    st = jrpc89_request_decode(&a, node, &r);
    if (st != JRPC89_OK)
    {
        fail("decode empty method");
    }
    if (r.method_len != 0)
    {
        fail("empty method len");
    }
    j89_arena_destroy(&a);
}

static void test_unknown_members(void)
{
    j89_arena a;
    j89_len node;
    jrpc89_request r;
    jrpc89_status st;

    j89_arena_init(&a);
    node = parse(
        "{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1,\"extra\":true}", &a);
    if (node == J89_BAD)
    {
        fail("parse unknown member");
        j89_arena_destroy(&a);
        return;
    }
    set_sentinel(&r);
    st = jrpc89_request_decode(&a, node, &r);
    if (st != JRPC89_OK)
    {
        fail("decode unknown member");
    }
    j89_arena_destroy(&a);
}

static void test_duplicate_keys(void)
{
    j89_arena a;
    j89_len node;
    const char *err;

    j89_arena_init(&a);
    node = parse("{\"jsonrpc\":\"2.0\",\"method\":\"a\",\"method\":\"b\"}", &a);
    if (node != J89_BAD)
    {
        fail("duplicate keys accepted");
    }
    err = j89_error(&a);
    if (err[0] == '\0')
    {
        fail("duplicate keys recorded no error");
    }
    j89_arena_destroy(&a);
}

static void test_failures(void)
{
    expect_bad("[]", JRPC89_EPROTO);
    expect_bad("{\"method\":\"m\"}", JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":\"1.0\",\"method\":\"m\"}", JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":2,\"method\":\"m\"}", JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":\"2.0\"}", JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":\"2.0\",\"method\":1}", JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"params\":null}",
               JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"params\":true}",
               JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"params\":1}",
               JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"params\":\"x\"}",
               JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":true}",
               JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1.5}",
               JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":[1]}",
               JRPC89_EPROTO);
    expect_bad("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":{\"a\":1}}",
               JRPC89_EPROTO);
}

static void test_bad_node(void)
{
    j89_arena a;
    jrpc89_request r;
    jrpc89_status st;

    j89_arena_init(&a);
    set_sentinel(&r);
    st = jrpc89_request_decode(&a, J89_BAD, &r);
    if (st != JRPC89_EPROTO)
    {
        fail("J89_BAD node");
    }
    if (!is_sentinel(&r))
    {
        fail("J89_BAD modified out");
    }
    j89_arena_destroy(&a);
}

static void test_null_arguments(void)
{
    j89_arena a;
    j89_len node;
    jrpc89_request r;
    jrpc89_status st;

    j89_arena_init(&a);
    node = parse("{\"jsonrpc\":\"2.0\",\"method\":\"m\"}", &a);
    if (node == J89_BAD)
    {
        fail("parse null args");
        j89_arena_destroy(&a);
        return;
    }
    set_sentinel(&r);
    st = jrpc89_request_decode(NULL, node, &r);
    if (st != JRPC89_EINVAL)
    {
        fail("NULL arena");
    }
    st = jrpc89_request_decode(&a, node, NULL);
    if (st != JRPC89_EINVAL)
    {
        fail("NULL out");
    }
    if (!is_sentinel(&r))
    {
        fail("NULL args modified out");
    }
    j89_arena_destroy(&a);
}

static void test_dirty_arena(void)
{
    j89_arena a;
    j89_len node;
    j89_len bad;
    jrpc89_request r;
    jrpc89_status st;

    j89_arena_init(&a);
    node = parse("{\"jsonrpc\":\"2.0\",\"method\":\"m\"}", &a);
    if (node == J89_BAD)
    {
        fail("parse dirty");
        j89_arena_destroy(&a);
        return;
    }
    bad = j89_string_new(&a, "\xff", 1);
    if (bad != J89_BAD)
    {
        fail("invalid utf8 accepted");
    }
    set_sentinel(&r);
    st = jrpc89_request_decode(&a, node, &r);
    if (st != JRPC89_EINVAL)
    {
        fail("dirty arena");
    }
    if (!is_sentinel(&r))
    {
        fail("dirty arena modified out");
    }
    j89_arena_destroy(&a);
}

static void test_round_trip(void)
{
    j89_arena a;
    j89_arena b;
    j89_len params;
    j89_len req;
    j89_len node;
    jrpc89_id id;
    jrpc89_request r;
    jrpc89_status st;
    char text[256];
    char pa[64];
    char pb[64];
    int rok;

    j89_arena_init(&a);
    params = j89_array_new(&a, 2);
    j89_array_set(&a, params, 0, j89_integer_new(&a, 7));
    j89_array_set(&a, params, 1, j89_string_new(&a, "x", 1));
    id.kind = JRPC89_ID_STRING;
    id.num = 0;
    id.str = "abc";
    id.len = 3;
    req = J89_BAD;
    st = jrpc89_request_new(&a, "m", 1, params, &id, &req);
    if (st != JRPC89_OK)
    {
        fail("round trip build");
        j89_arena_destroy(&a);
        return;
    }
    rok = render_to(&a, req, text, sizeof text);
    if (rok != 0)
    {
        fail("round trip render");
        j89_arena_destroy(&a);
        return;
    }
    j89_arena_init(&b);
    node = parse(text, &b);
    if (node == J89_BAD)
    {
        fail("round trip parse");
        j89_arena_destroy(&b);
        j89_arena_destroy(&a);
        return;
    }
    set_sentinel(&r);
    st = jrpc89_request_decode(&b, node, &r);
    if (st != JRPC89_OK)
    {
        fail("round trip decode");
    }
    if (r.method_len != 1)
    {
        fail("round trip method len");
    }
    if (r.params == J89_BAD)
    {
        fail("round trip params");
    }
    if (!jrpc89_id_equal(&r.id, &id))
    {
        fail("round trip id");
    }
    rok = render_to(&a, params, pa, sizeof pa);
    if (rok != 0)
    {
        fail("round trip params render a");
    }
    rok = render_to(&b, r.params, pb, sizeof pb);
    if (rok != 0)
    {
        fail("round trip params render b");
    }
    if (strcmp(pa, pb) != 0)
    {
        fail("round trip params bytes");
    }
    j89_arena_destroy(&b);
    j89_arena_destroy(&a);
}

int main(void)
{
    test_minimal();
    test_notification();
    test_params_forms();
    test_id_forms();
    test_method_strings();
    test_unknown_members();
    test_duplicate_keys();
    test_failures();
    test_bad_node();
    test_null_arguments();
    test_dirty_arena();
    test_round_trip();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_request_decode: ok\n");
    return 0;
}
