/* test_builder.c - unit tests for the libj89 public builder API. */
#include <stdio.h>
#include <string.h>

#include "j89.h"

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static void expect_render(const char *label, j89_arena *out,
                          const char *expected)
{
    j89_len got;
    const char *s;
    got = out->off;
    s = (const char *)out->mem;
    if (got != (j89_len)strlen(expected))
    {
        fail(label);
        fprintf(stderr, "  expected <%s>, got length %lu\n", expected,
                (unsigned long)got);
        return;
    }
    if (strncmp(s, expected, (size_t)got) != 0)
    {
        fail(label);
        fprintf(stderr, "  expected <%s>, got <%.*s>\n", expected, (int)got, s);
    }
}

static void test_build_minimal_object(void)
{
    j89_arena a;
    j89_arena out;
    j89_len root;
    j89_len val;
    j89_arena_init(&a);
    j89_arena_init(&out);
    root = j89_object_new(&a, 1);
    val = j89_string_new(&a, "x", 1);
    j89_object_set(&a, root, 0, "a", 1, val);
    if (j89_render(&a, root, 1, &out) != 0)
    {
        fail("minimal object render");
    }
    else
    {
        expect_render("minimal object", &out, "{\"a\":\"x\"}");
    }
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

static void test_build_nested(void)
{
    j89_arena a;
    j89_arena out;
    j89_len root;
    j89_len arr;
    j89_len obj;
    j89_len i1;
    j89_len i2;
    j89_arena_init(&a);
    j89_arena_init(&out);
    arr = j89_array_new(&a, 2);
    i1 = j89_integer_new(&a, 1);
    i2 = j89_integer_new(&a, 2);
    j89_array_set(&a, arr, 0, i1);
    j89_array_set(&a, arr, 1, i2);
    obj = j89_object_new(&a, 1);
    j89_object_set(&a, obj, 0, "b", 1, arr);
    root = j89_object_new(&a, 1);
    j89_object_set(&a, root, 0, "a", 1, obj);
    if (j89_render(&a, root, 1, &out) != 0)
    {
        fail("nested render");
    }
    else
    {
        expect_render("nested", &out, "{\"a\":{\"b\":[1,2]}}");
    }
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

static void test_build_escaping(void)
{
    j89_arena a;
    j89_arena out;
    j89_len root;
    j89_len val;
    static const char tricky[] = "a\"b\\c\nd\te\007";
    j89_arena_init(&a);
    j89_arena_init(&out);
    root = j89_object_new(&a, 1);
    val = j89_string_new(&a, tricky, (j89_len)(sizeof(tricky) - 1));
    j89_object_set(&a, root, 0, "k", 1, val);
    if (j89_render(&a, root, 1, &out) != 0)
    {
        fail("escaping render");
    }
    else
    {
        expect_render("escaping", &out,
                      "{\"k\":\"a\\\"b\\\\c\\nd\\te\\u0007\"}");
    }
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

static void test_build_numbers(void)
{
    j89_arena a;
    j89_arena out;
    j89_len root;
    j89_len iv;
    j89_len dv;
    j89_arena_init(&a);
    j89_arena_init(&out);
    root = j89_object_new(&a, 2);
    iv = j89_integer_new(&a, 42);
    dv = j89_double_new(&a, 0.5);
    j89_object_set(&a, root, 0, "i", 1, iv);
    j89_object_set(&a, root, 1, "d", 1, dv);
    if (j89_render(&a, root, 1, &out) != 0)
    {
        fail("numbers render");
    }
    else
    {
        expect_render("numbers", &out, "{\"i\":42,\"d\":0.5}");
    }
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

static void test_build_bool(void)
{
    j89_arena a;
    j89_arena out;
    j89_len root;
    j89_arena_init(&a);
    j89_arena_init(&out);
    root = j89_object_new(&a, 2);
    j89_object_set(&a, root, 0, "t", 1, j89_bool_new(&a, 1));
    j89_object_set(&a, root, 1, "f", 1, j89_bool_new(&a, 0));
    if (j89_render(&a, root, 1, &out) != 0)
    {
        fail("bool render");
    }
    else
    {
        expect_render("bool", &out, "{\"t\":true,\"f\":false}");
    }
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

static void test_build_roundtrip(void)
{
    j89_arena a;
    j89_arena out;
    j89_arena back;
    j89_len root;
    j89_len parsed;
    j89_arena_init(&a);
    j89_arena_init(&out);
    j89_arena_init(&back);
    root = j89_object_new(&a, 2);
    j89_object_set(&a, root, 0, "i", 1, j89_integer_new(&a, 7));
    j89_object_set(&a, root, 1, "s", 1, j89_string_new(&a, "hi", 2));
    if (j89_render(&a, root, 1, &out) != 0)
    {
        fail("roundtrip render");
    }
    else
    {
        const char *s;
        s = (const char *)out.mem;
        parsed = j89_parse(s, out.off, &back);
        if (parsed == J89_BAD)
        {
            fail("roundtrip parse");
        }
        else if (j89_kind_of(&back, parsed) != J89_OBJECT)
        {
            fail("roundtrip kind");
        }
        else if (j89_object_length(&back, parsed) != 2)
        {
            fail("roundtrip length");
        }
    }
    j89_arena_destroy(&back);
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

static void test_build_utf8_string(void)
{
    j89_arena a;
    j89_arena out;
    j89_arena back;
    j89_len root;
    j89_len val;
    j89_len parsed;
    j89_len node;
    static const char u[] = "h\303\251llo"; /* "héllo" */
    j89_arena_init(&a);
    j89_arena_init(&out);
    j89_arena_init(&back);
    root = j89_object_new(&a, 1);
    val = j89_string_new(&a, u, (j89_len)(sizeof(u) - 1));
    j89_object_set(&a, root, 0, "s", 1, val);
    if (j89_render(&a, root, 1, &out) != 0)
    {
        fail("utf8 render");
    }
    else
    {
        const char *s;
        const char *rv;
        j89_len rl;
        s = (const char *)out.mem;
        parsed = j89_parse(s, out.off, &back);
        if (parsed == J89_BAD)
        {
            fail("utf8 parse");
        }
        else
        {
            node = j89_object_find(&back, parsed, "s");
            rv = j89_string_value(&back, node);
            rl = j89_string_length(&back, node);
            if (rl != (j89_len)(sizeof(u) - 1))
            {
                fail("utf8 length");
            }
            else if (memcmp(rv, u, (size_t)rl) != 0)
            {
                fail("utf8 bytes");
            }
        }
    }
    j89_arena_destroy(&back);
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

static void test_build_bad_inputs(void)
{
    j89_arena a;
    j89_len root;
    j89_arena_init(&a);
    root = j89_object_new(&a, 1);
    j89_object_set(&a, root, 0, "k", 1, J89_BAD);
    if (!j89_failed(&a))
    {
        fail("object_set bad value marks failed");
    }
    j89_arena_destroy(&a);

    j89_arena_init(&a);
    root = j89_object_new(&a, 1);
    j89_array_set(&a, J89_BAD, 0, j89_integer_new(&a, 1));
    if (!j89_failed(&a))
    {
        fail("array_set bad array marks failed");
    }
    j89_arena_destroy(&a);
}

static void test_build_invalid_utf8_string(void)
{
    j89_arena a;
    j89_len node;
    j89_len off_before;
    static const char bad[] = "\xc3\x28"; /* truncated 2-byte sequence */
    j89_arena_init(&a);
    off_before = a.off;
    node = j89_string_new(&a, bad, (j89_len)(sizeof(bad) - 1));
    if (node != J89_BAD)
    {
        fail("invalid utf8 string is rejected");
    }
    if (!j89_failed(&a))
    {
        fail("invalid utf8 string marks arena failed");
    }
    if (a.off != off_before)
    {
        fail("invalid utf8 string consumes no arena storage");
    }
    if (strstr(j89_error(&a), "UTF-8") == NULL)
    {
        fail("invalid utf8 string error names UTF-8");
    }
    j89_arena_destroy(&a);
}

static void test_build_invalid_utf8_key(void)
{
    j89_arena a;
    j89_len root;
    j89_len val;
    j89_len off_before;
    const char *k;
    j89_len kl;
    static const char badkey[] = "\x80"; /* lone continuation byte */
    j89_arena_init(&a);
    root = j89_object_new(&a, 1);
    val = j89_integer_new(&a, 7);
    j89_object_set(&a, root, 0, "ok", 2, val);
    off_before = a.off;
    j89_object_set(&a, root, 0, badkey, 1, val);
    if (!j89_failed(&a))
    {
        fail("invalid utf8 key marks arena failed");
    }
    if (a.off != off_before)
    {
        fail("invalid utf8 key consumes no arena storage");
    }
    if (strstr(j89_error(&a), "UTF-8") == NULL)
    {
        fail("invalid utf8 key error names UTF-8");
    }
    k = j89_object_key(&a, root, 0);
    kl = j89_object_key_length(&a, root, 0);
    if (kl != 2 || k[0] != 'o' || k[1] != 'k')
    {
        fail("invalid utf8 key leaves the member slot unchanged");
    }
    if (j89_object_value(&a, root, 0) != val)
    {
        fail("invalid utf8 key leaves the member value unchanged");
    }
    j89_arena_destroy(&a);
}

static void test_build_embedded_nul_key(void)
{
    j89_arena a;
    j89_len root;
    j89_len val;
    const char *k;
    j89_len kl;
    static const char key[] = {'a', '\0', 'b'};
    j89_arena_init(&a);
    root = j89_object_new(&a, 1);
    val = j89_integer_new(&a, 1);
    j89_object_set(&a, root, 0, key, 3, val);
    if (j89_failed(&a))
    {
        fail("embedded nul key is accepted");
    }
    k = j89_object_key(&a, root, 0);
    kl = j89_object_key_length(&a, root, 0);
    if (kl != 3)
    {
        fail("embedded nul key length");
    }
    if (k[0] != 'a' || k[1] != '\0' || k[2] != 'b' || k[3] != '\0')
    {
        fail("embedded nul key bytes");
    }
    j89_arena_destroy(&a);
}

static void test_build_null(void)
{
    j89_arena a;
    j89_arena out;
    j89_len node;
    j89_kind k;
    j89_arena_init(&a);
    j89_arena_init(&out);
    node = j89_null_new(&a);
    if (node == J89_BAD)
    {
        fail("null node");
    }
    else
    {
        k = j89_kind_of(&a, node);
        if (k != J89_NULL)
        {
            fail("null kind");
        }
        else if (j89_render(&a, node, 1, &out) != 0)
        {
            fail("null render");
        }
        else
        {
            expect_render("null", &out, "null");
        }
    }
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

int main(void)
{
    failures = 0;
    test_build_minimal_object();
    test_build_nested();
    test_build_escaping();
    test_build_numbers();
    test_build_bool();
    test_build_null();
    test_build_roundtrip();
    test_build_utf8_string();
    test_build_bad_inputs();
    test_build_invalid_utf8_string();
    test_build_invalid_utf8_key();
    test_build_embedded_nul_key();
    if (failures != 0)
    {
        fprintf(stderr, "test_builder: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_builder: OK\n");
    return 0;
}
