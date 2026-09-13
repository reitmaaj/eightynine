/* test_str_storage.c - owned str89 storage: embedded NUL, distinct keys,
 * surrogate escapes, and exact parse/serialize round trips. */
#include <stdio.h>
#include <string.h>

#include "j89.h"

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static void check(int cond, const char *what)
{
    if (!cond)
    {
        fail(what);
    }
}

static j89_len parse_text(const char *text, j89_arena *a)
{
    return j89_parse(text, (j89_len)strlen(text), a);
}

static void check_bytes(const char *got, j89_len gotlen,
                        const unsigned char *want, size_t wantlen,
                        const char *what)
{
    check(gotlen == (j89_len)wantlen, what);
    if (gotlen != (j89_len)wantlen)
    {
        return;
    }
    if (wantlen != 0)
    {
        check(memcmp(got, want, wantlen) == 0, what);
    }
}

static void test_nul_value_roundtrip(void)
{
    static const unsigned char text[] = {'a', 0x00, 'b'};
    j89_arena a;
    j89_arena out;
    j89_arena back;
    j89_len root;
    j89_len s;
    j89_len val;
    const char *v;
    j89_len n;
    j89_arena_init(&a);
    j89_arena_init(&out);
    j89_arena_init(&back);
    root = j89_object_new(&a, 1);
    s = j89_string_new(&a, (const char *)text, 3);
    check(s != J89_BAD, "nul value: create");
    j89_object_set(&a, root, 0, "k", 1, s);
    check(j89_render(&a, root, 1, &out) == 0, "nul value: render");
    root = j89_parse((const char *)out.mem, out.off, &back);
    check(root != J89_BAD, "nul value: parse");
    val = j89_object_value(&back, root, 0);
    v = j89_string_value(&back, val);
    n = j89_string_length(&back, val);
    check_bytes(v, n, text, sizeof(text), "nul value: bytes");
    check(v[n] == '\0', "nul value: terminator");
    j89_arena_destroy(&back);
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

static void test_nul_keys_distinct(void)
{
    static const char doc[] = "{\"a\\u0000x\":1,\"a\\u0000y\":2}";
    j89_arena a;
    j89_len root;
    const char *k;
    j89_len kl;
    j89_arena_init(&a);
    root = parse_text(doc, &a);
    check(root != J89_BAD, "nul keys: parse");
    check(j89_object_length(&a, root) == 2, "nul keys: two members");
    k = j89_object_key(&a, root, 0);
    kl = j89_object_key_length(&a, root, 0);
    check_bytes(k, kl, (const unsigned char *)"a\x00x", 3,
                "nul keys: first key");
    check(k[3] == '\0', "nul keys: first terminator");
    k = j89_object_key(&a, root, 1);
    kl = j89_object_key_length(&a, root, 1);
    check_bytes(k, kl, (const unsigned char *)"a\x00y", 3,
                "nul keys: second key");
    check(j89_object_find(&a, root, "a") == J89_BAD,
          "nul keys: no prefix lookup");
    j89_arena_destroy(&a);
}

static void test_canonical_keys_distinct(void)
{
    static const char doc[] = "{\"\\u00e9\":1,\"e\\u0301\":2}";
    j89_arena a;
    j89_len root;
    const char *k;
    j89_len kl;
    j89_arena_init(&a);
    root = parse_text(doc, &a);
    check(root != J89_BAD, "canonical keys: parse");
    check(j89_object_length(&a, root) == 2, "canonical keys: two members");
    k = j89_object_key(&a, root, 0);
    kl = j89_object_key_length(&a, root, 0);
    check_bytes(k, kl, (const unsigned char *)"\xC3\xA9", 2,
                "canonical keys: precomposed");
    k = j89_object_key(&a, root, 1);
    kl = j89_object_key_length(&a, root, 1);
    check_bytes(k, kl, (const unsigned char *)"e\xCC\x81", 3,
                "canonical keys: decomposed");
    j89_arena_destroy(&a);
}

static void test_surrogate_roundtrip(void)
{
    static const unsigned char key_bytes[] = {0xF0, 0x9D, 0x84, 0x9E};
    static const unsigned char val_bytes[] = {0xF4, 0x8F, 0xBF, 0xBF};
    static const char doc[] = "{\"\\ud834\\udd1e\":\"\\udbff\\udfff\"}";
    j89_arena a;
    j89_len root;
    j89_len val;
    const char *k;
    j89_len kl;
    const char *v;
    j89_len vl;
    j89_arena_init(&a);
    root = parse_text(doc, &a);
    check(root != J89_BAD, "surrogate: parse");
    k = j89_object_key(&a, root, 0);
    kl = j89_object_key_length(&a, root, 0);
    check_bytes(k, kl, key_bytes, sizeof(key_bytes), "surrogate: key");
    val = j89_object_value(&a, root, 0);
    v = j89_string_value(&a, val);
    vl = j89_string_length(&a, val);
    check_bytes(v, vl, val_bytes, sizeof(val_bytes), "surrogate: value");
    j89_arena_destroy(&a);
}

static void test_escape_roundtrip(void)
{
    static const unsigned char bytes[] = {'"',  '\\', '\n', '\t', 0x00, 0xC3,
                                          0xA9, 0xF0, 0x90, 0x8D, 0x88};
    j89_arena a;
    j89_arena out;
    j89_arena back;
    j89_len root;
    j89_len s;
    j89_len val;
    const char *v;
    j89_len n;
    j89_arena_init(&a);
    j89_arena_init(&out);
    j89_arena_init(&back);
    s = j89_string_new(&a, (const char *)bytes, (j89_len)sizeof(bytes));
    check(s != J89_BAD, "escape: create");
    root = j89_array_new(&a, 1);
    j89_array_set(&a, root, 0, s);
    check(j89_render(&a, root, 1, &out) == 0, "escape: render");
    root = j89_parse((const char *)out.mem, out.off, &back);
    check(root != J89_BAD, "escape: reparse");
    val = j89_array_get(&back, root, 0);
    v = j89_string_value(&back, val);
    n = j89_string_length(&back, val);
    check_bytes(v, n, bytes, sizeof(bytes), "escape: round trip bytes");
    j89_arena_destroy(&back);
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

static void test_builder_nul_key(void)
{
    j89_arena a;
    j89_arena out;
    j89_arena back;
    j89_len root;
    j89_len one;
    j89_len two;
    const char *k;
    j89_len kl;
    j89_arena_init(&a);
    j89_arena_init(&out);
    j89_arena_init(&back);
    root = j89_object_new(&a, 2);
    one = j89_integer_new(&a, 1);
    two = j89_integer_new(&a, 2);
    j89_object_set(&a, root, 0, "a\x00x", 3, one);
    j89_object_set(&a, root, 1, "a\x00y", 3, two);
    check(j89_render(&a, root, 1, &out) == 0, "builder nul: render");
    root = j89_parse((const char *)out.mem, out.off, &back);
    check(root != J89_BAD, "builder nul: reparse");
    check(j89_object_length(&back, root) == 2, "builder nul: two members");
    k = j89_object_key(&back, root, 0);
    kl = j89_object_key_length(&back, root, 0);
    check_bytes(k, kl, (const unsigned char *)"a\x00x", 3,
                "builder nul: first key");
    j89_arena_destroy(&back);
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

int main(void)
{
    test_nul_value_roundtrip();
    test_nul_keys_distinct();
    test_canonical_keys_distinct();
    test_surrogate_roundtrip();
    test_escape_roundtrip();
    test_builder_nul_key();
    if (failures != 0)
    {
        fprintf(stderr, "%d checks FAILED\n", failures);
        return 1;
    }
    printf("test_str_storage: OK\n");
    return 0;
}
