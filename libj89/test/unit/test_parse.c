/* test_parse.c - unit tests for libj89. */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../src/internal.h"
#include "j89.h"

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

static int expect_bad(const char *label, const char *s)
{
    j89_arena a;
    j89_len root;
    j89_arena_init(&a);
    root = parse(s, &a);
    if (root != J89_BAD)
    {
        fail(label);
        j89_arena_destroy(&a);
        return 1;
    }
    j89_arena_destroy(&a);
    return 0;
}

static void test_minimal_object(void)
{
    j89_arena a;
    j89_len root;
    j89_len node;
    j89_kind k;
    j89_int v;
    j89_arena_init(&a);
    root = parse("{\"a\":1}", &a);
    if (root == J89_BAD)
    {
        fail("minimal object parse");
        j89_arena_destroy(&a);
        return;
    }
    k = j89_kind_of(&a, root);
    if (k != J89_OBJECT)
    {
        fail("root kind object");
    }
    node = j89_object_find(&a, root, "a");
    if (node == J89_BAD)
    {
        fail("find a");
    }
    else
    {
        k = j89_kind_of(&a, node);
        if (k != J89_INTEGER)
        {
            fail("a kind integer");
        }
        v = j89_int_value(&a, node);
        if (v != 1)
        {
            fail("a value 1");
        }
    }
    j89_arena_destroy(&a);
}

static void test_nested(void)
{
    j89_arena a;
    j89_len root;
    j89_len arr;
    j89_len elem;
    j89_len n;
    j89_kind k;
    j89_arena_init(&a);
    root = parse("{\"a\":[1,2,{\"b\":null}],\"c\":true}", &a);
    if (root == J89_BAD)
    {
        fail("nested parse");
        j89_arena_destroy(&a);
        return;
    }
    arr = j89_object_find(&a, root, "a");
    n = j89_array_length(&a, arr);
    if (n != 3)
    {
        fail("array length 3");
    }
    elem = j89_array_get(&a, arr, 2);
    k = j89_kind_of(&a, elem);
    if (k != J89_OBJECT)
    {
        fail("array[2] object");
    }
    elem = j89_object_find(&a, elem, "b");
    k = j89_kind_of(&a, elem);
    if (k != J89_NULL)
    {
        fail("b null");
    }
    j89_arena_destroy(&a);
}

static void test_string_escapes(void)
{
    j89_arena a;
    j89_len root;
    j89_len node;
    const char *v;
    j89_len n;
    j89_arena_init(&a);
    root = parse("\"a\\\"b\\n\\u0041\"", &a);
    if (root == J89_BAD)
    {
        fail("escape parse");
        j89_arena_destroy(&a);
        return;
    }
    node = root;
    v = j89_string_value(&a, node);
    n = j89_string_length(&a, node);
    if (n != 5)
    {
        fail("escape length");
    }
    if (v[0] != 'a' || v[1] != '"' || v[2] != 'b' || v[3] != '\n' ||
        v[4] != 'A')
    {
        fail("escape bytes");
    }
    j89_arena_destroy(&a);
}

static void test_surrogate(void)
{
    j89_arena a;
    j89_len root;
    const char *v;
    j89_len n;
    j89_arena_init(&a);
    root = parse("\"\\ud83d\\ude00\"", &a);
    if (root == J89_BAD)
    {
        fail("surrogate parse");
        j89_arena_destroy(&a);
        return;
    }
    v = j89_string_value(&a, root);
    n = j89_string_length(&a, root);
    if (n != 4)
    {
        fail("surrogate length 4");
    }
    if ((unsigned char)v[0] != 0xF0 || (unsigned char)v[1] != 0x9F)
    {
        fail("surrogate utf8 prefix");
    }
    j89_arena_destroy(&a);
}

static void test_negative_int(void)
{
    j89_arena a;
    j89_len root;
    j89_int v;
    j89_kind k;
    j89_arena_init(&a);
    root = parse("-42", &a);
    if (root == J89_BAD)
    {
        fail("negative int parse");
        j89_arena_destroy(&a);
        return;
    }
    k = j89_kind_of(&a, root);
    if (k != J89_INTEGER)
    {
        fail("negative kind");
    }
    v = j89_int_value(&a, root);
    if (v != -42)
    {
        fail("negative value");
    }
    j89_arena_destroy(&a);
}

static void test_render_roundtrip(void)
{
    j89_arena a;
    j89_arena out;
    j89_len root;
    const char *txt;
    j89_len n;
    j89_arena_init(&a);
    j89_arena_init(&out);
    root = parse("{\"a\":1,\"b\":[true,null]}", &a);
    if (root == J89_BAD)
    {
        fail("roundtrip parse");
        j89_arena_destroy(&a);
        j89_arena_destroy(&out);
        return;
    }
    if (j89_render(&a, root, 1, &out) != 0)
    {
        fail("render");
        j89_arena_destroy(&a);
        j89_arena_destroy(&out);
        return;
    }
    txt = (const char *)out.mem;
    n = out.off;
    if (n == 0 || strncmp(txt, "{\"a\":1,\"b\":[true,null]}", n) != 0)
    {
        fail("render output");
    }
    j89_arena_destroy(&a);
    j89_arena_destroy(&out);
}

static void test_rejections(void)
{
    expect_bad("reject malformed number dot then nothing", "1.");
    expect_bad("reject malformed number leading dot", ".5");
    expect_bad("reject malformed number bare exp", "1e");
    expect_bad("reject malformed number exp plus", "1e+");
    expect_bad("reject leading zero", "01");
    expect_bad("reject bare minus", "-");
    expect_bad("reject Infinity", "Infinity");
    expect_bad("reject NaN", "NaN");
    expect_bad("reject trailing", "{} x");
    expect_bad("reject malformed", "{\"a\":}");
    expect_bad("reject unterminated", "\"abc");
    expect_bad("reject lone low surrogate", "\"\\udc00\"");
    expect_bad("reject lone high surrogate", "\"\\ud800\"");
    expect_bad("reject empty", "");
}

static void test_float(void)
{
    j89_arena a;
    j89_len root;
    j89_kind k;
    double v;
    j89_arena_init(&a);
    root = parse("1.5", &a);
    if (root == J89_BAD)
    {
        fail("float parse");
        j89_arena_destroy(&a);
        return;
    }
    k = j89_kind_of(&a, root);
    if (k != J89_FLOAT)
    {
        fail("float kind");
    }
    v = j89_double_value(&a, root);
    if (v != 1.5)
    {
        fail("float value 1.5");
    }
    j89_arena_destroy(&a);

    j89_arena_init(&a);
    root = parse("-0.5", &a);
    if (root == J89_BAD)
    {
        fail("neg float parse");
        j89_arena_destroy(&a);
        return;
    }
    v = j89_double_value(&a, root);
    if (v != -0.5)
    {
        fail("neg float value");
    }
    j89_arena_destroy(&a);

    j89_arena_init(&a);
    root = parse("1e3", &a);
    if (root == J89_BAD)
    {
        fail("exp float parse");
        j89_arena_destroy(&a);
        return;
    }
    k = j89_kind_of(&a, root);
    if (k != J89_FLOAT)
    {
        fail("exp float kind");
    }
    v = j89_double_value(&a, root);
    if (v != 1000.0)
    {
        fail("exp float value 1e3");
    }
    j89_arena_destroy(&a);

    j89_arena_init(&a);
    root = parse("123e65", &a);
    if (root == J89_BAD)
    {
        fail("big exp float parse");
        j89_arena_destroy(&a);
        return;
    }
    v = j89_double_value(&a, root);
    if (v != 1.23e67)
    {
        fail("big exp float value");
    }
    j89_arena_destroy(&a);

    expect_bad("reject overflow double", "1e999");
}

static void test_int_stays_int(void)
{
    j89_arena a;
    j89_len root;
    j89_kind k;
    j89_arena_init(&a);
    root = parse("42", &a);
    if (root == J89_BAD)
    {
        fail("int parse");
        j89_arena_destroy(&a);
        return;
    }
    k = j89_kind_of(&a, root);
    if (k != J89_INTEGER)
    {
        fail("integer literal stays INTEGER");
    }
    j89_arena_destroy(&a);
}

static void test_utf8(void)
{
    j89_arena a;
    j89_len root;
    j89_arena_init(&a);
    root = parse("\"caf\\u00e9\"", &a);
    if (root == J89_BAD)
    {
        fail("escaped non-ascii parse");
        j89_arena_destroy(&a);
        return;
    }
    j89_arena_destroy(&a);

    j89_arena_init(&a);
    root = parse("\"caf\xc3\xa9\"", &a);
    if (root == J89_BAD)
    {
        fail("valid 2-byte utf8 parse");
        j89_arena_destroy(&a);
        return;
    }
    j89_arena_destroy(&a);

    expect_bad("reject lone continuation byte", "\"\xc3\"");
    expect_bad("reject truncated utf8", "\"\xc3\x28\"");
    expect_bad("reject overlong utf8", "\"\xc0\xaf\"");
    expect_bad("reject encoded surrogate", "\"\xed\xa0\x80\"");
    expect_bad("reject above U+10FFFF", "\"\xf4\x90\x80\x80\"");
    expect_bad("reject invalid utf8 in key", "{\"\xc3\":1}");
}

static void test_bom(void)
{
    j89_arena a;
    j89_len root;
    j89_arena_init(&a);
    root = j89_parse("\xEF\xBB\xBF{}", (j89_len)5, &a);
    if (root == J89_BAD)
    {
        fail("BOM prefix parse");
        j89_arena_destroy(&a);
        return;
    }
    j89_arena_destroy(&a);

    expect_bad("reject partial BOM", "\xEF{}");
}

static void test_duplicate_key(void)
{
    j89_arena a;
    j89_len root;
    j89_arena_init(&a);
    root = parse("{\"a\":1,\"a\":2}", &a);
    if (root != J89_BAD)
    {
        fail("reject duplicate key");
        j89_arena_destroy(&a);
        return;
    }
    j89_arena_destroy(&a);

    j89_arena_init(&a);
    root = parse("{\"a\":1,\"b\":2}", &a);
    if (root == J89_BAD)
    {
        fail("accept distinct keys");
        j89_arena_destroy(&a);
        return;
    }
    j89_arena_destroy(&a);
}

static void test_int_limits(void)
{
    j89_arena a;
    j89_len root;
    j89_int v;
    j89_kind k;
    j89_arena_init(&a);
    root = parse("9007199254740992", &a);
    if (root == J89_BAD)
    {
        fail("2^53 parse");
        j89_arena_destroy(&a);
        return;
    }
    k = j89_kind_of(&a, root);
    if (k != J89_INTEGER)
    {
        fail("2^53 kind");
    }
    v = j89_int_value(&a, root);
    if (v != 9007199254740992.0)
    {
        fail("2^53 value");
    }
    j89_arena_destroy(&a);

    j89_arena_init(&a);
    root = parse("-9007199254740992", &a);
    if (root == J89_BAD)
    {
        fail("-2^53 parse");
        j89_arena_destroy(&a);
        return;
    }
    k = j89_kind_of(&a, root);
    if (k != J89_INTEGER)
    {
        fail("-2^53 kind");
    }
    v = j89_int_value(&a, root);
    if (v != -9007199254740992.0)
    {
        fail("-2^53 value");
    }
    j89_arena_destroy(&a);

    expect_bad("reject 2^53+1", "9007199254740993");
    expect_bad("reject -(2^53+1)", "-9007199254740993");
    expect_bad("reject LONG_MAX", "9223372036854775807");
    expect_bad("reject LONG_MIN", "-9223372036854775808");

    j89_arena_init(&a);
    root = parse("-0", &a);
    if (root == J89_BAD)
    {
        fail("neg zero parse");
        j89_arena_destroy(&a);
        return;
    }
    v = j89_int_value(&a, root);
    if (v != 0)
    {
        fail("neg zero value");
    }
    j89_arena_destroy(&a);
}

static void test_string_terminated(void)
{
    j89_arena a;
    j89_len root;
    const char *v;
    j89_len n;
    j89_arena_init(&a);
    root = parse("\"abc\"", &a);
    if (root == J89_BAD)
    {
        fail("terminated parse");
        j89_arena_destroy(&a);
        return;
    }
    v = j89_string_value(&a, root);
    n = j89_string_length(&a, root);
    if (n != 3)
    {
        fail("terminated length");
    }
    if (v[3] != '\0')
    {
        fail("string value not NUL-terminated");
    }
    j89_arena_destroy(&a);
}

static void test_embedded_nul(void)
{
    j89_arena a;
    j89_len root;
    const char *v;
    j89_len n;
    j89_arena_init(&a);
    root = parse("\"\\u0000\"", &a);
    if (root == J89_BAD)
    {
        fail("embedded nul parse");
        j89_arena_destroy(&a);
        return;
    }
    v = j89_string_value(&a, root);
    n = j89_string_length(&a, root);
    if (n != 1)
    {
        fail("embedded nul length");
    }
    if ((unsigned char)v[0] != 0)
    {
        fail("embedded nul byte");
    }
    if (v[1] != '\0')
    {
        fail("embedded nul terminator");
    }
    j89_arena_destroy(&a);
}

static void test_object_key_terminated(void)
{
    j89_arena a;
    j89_len root;
    const char *k;
    j89_len n;
    j89_arena_init(&a);
    root = parse("{\"key\":1}", &a);
    if (root == J89_BAD)
    {
        fail("key parse");
        j89_arena_destroy(&a);
        return;
    }
    n = j89_object_length(&a, root);
    if (n != 1)
    {
        fail("key count");
        j89_arena_destroy(&a);
        return;
    }
    k = j89_object_key(&a, root, 0);
    if (k[0] != 'k' || k[1] != 'e' || k[2] != 'y')
    {
        fail("key bytes");
    }
    if (k[3] != '\0')
    {
        fail("object key not NUL-terminated");
    }
    j89_arena_destroy(&a);
}

/* Build a chain of nested arrays: base empty array plus `levels` wrappers.
 * Total depth is 1 + levels. */
static j89_len build_chain(j89_arena *a, long levels)
{
    j89_len node;
    j89_len outer;
    long i;
    node = j89_make_array(a, 0);
    if (node == J89_BAD)
    {
        return J89_BAD;
    }
    i = 0;
    while (i < levels)
    {
        outer = j89_make_array(a, 1);
        if (outer == J89_BAD)
        {
            return J89_BAD;
        }
        j89_array_set_child(a, outer, 0, node);
        node = outer;
        i = i + 1;
    }
    return node;
}

static void test_render_depth_guard(void)
{
    j89_arena a;
    j89_arena out;
    j89_len node;
    int rc;
    j89_arena_init(&a);
    j89_arena_init(&out);
    node = build_chain(&a, (long)J89_MAX_DEPTH);
    if (node == J89_BAD)
    {
        fail("render over build");
        j89_arena_destroy(&a);
        j89_arena_destroy(&out);
        return;
    }
    rc = j89_render(&a, node, 1, &out);
    if (rc == 0)
    {
        fail("render over-depth should fail");
    }
    j89_arena_destroy(&a);
    j89_arena_destroy(&out);

    j89_arena_init(&a);
    j89_arena_init(&out);
    node = build_chain(&a, (long)(J89_MAX_DEPTH - 1));
    if (node == J89_BAD)
    {
        fail("render at build");
        j89_arena_destroy(&a);
        j89_arena_destroy(&out);
        return;
    }
    rc = j89_render(&a, node, 1, &out);
    if (rc != 0)
    {
        fail("render at-depth should succeed");
    }
    j89_arena_destroy(&a);
    j89_arena_destroy(&out);
}

int main(void)
{
    test_minimal_object();
    test_nested();
    test_string_escapes();
    test_surrogate();
    test_negative_int();
    test_float();
    test_int_stays_int();
    test_utf8();
    test_bom();
    test_duplicate_key();
    test_render_roundtrip();
    test_rejections();
    test_int_limits();
    test_string_terminated();
    test_embedded_nul();
    test_object_key_terminated();
    test_render_depth_guard();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_parse: ok\n");
    return 0;
}
