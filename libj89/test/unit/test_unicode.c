/* test_unicode.c - Unicode delegation matrix for libj89.
 *
 * Raw UTF-8 boundaries, malformed raw UTF-8, BMP escapes, surrogate pairs,
 * malformed escapes, raw/escape equivalence, object keys, and render
 * round-trips. All expected bytes come from the Unicode scalar definitions;
 * the parser must delegate scalar validity to libu89 and own only the JSON
 * lexical rules. */
#include <stdio.h>
#include <string.h>

#include "j89.h"

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static const unsigned char U_0000[] = {0x00};
static const unsigned char U_007F[] = {0x7F};
static const unsigned char U_0080[] = {0xC2, 0x80};
static const unsigned char U_07FF[] = {0xDF, 0xBF};
static const unsigned char U_0800[] = {0xE0, 0xA0, 0x80};
static const unsigned char U_D7FF[] = {0xED, 0x9F, 0xBF};
static const unsigned char U_E000[] = {0xEE, 0x80, 0x80};
static const unsigned char U_FFFF[] = {0xEF, 0xBF, 0xBF};
static const unsigned char U_10000[] = {0xF0, 0x90, 0x80, 0x80};
static const unsigned char U_103FF[] = {0xF0, 0x90, 0x8F, 0xBF};
static const unsigned char U_10FC00[] = {0xF4, 0x8F, 0xB0, 0x80};
static const unsigned char U_10FFFF[] = {0xF4, 0x8F, 0xBF, 0xBF};
static const unsigned char U_1F600[] = {0xF0, 0x9F, 0x98, 0x80};

static const char E_0000[] = "\\u0000";
static const char E_007F[] = "\\u007f";
static const char E_0080[] = "\\u0080";
static const char E_07FF[] = "\\u07ff";
static const char E_D7FF[] = "\\ud7ff";
static const char E_E000[] = "\\ue000";
static const char E_FFFF[] = "\\uffff";
static const char E_10000[] = "\\ud800\\udc00";
static const char E_103FF[] = "\\ud800\\udfff";
static const char E_10FC00[] = "\\udbff\\udc00";
static const char E_10FFFF[] = "\\udbff\\udfff";
static const char E_1F600[] = "\\ud83d\\ude00";

struct bad_case
{
    const char *text;
    j89_len len;
    const char *label;
};

/* Raw malformed UTF-8 inside a quoted string. */
static const struct bad_case BAD_RAW[] = {
    {"\"\x80\"", 3, "lone continuation 80"},
    {"\"\xbf\"", 3, "lone continuation BF"},
    {"\"\xc0\xaf\"", 4, "C0 overlong"},
    {"\"\xc1\xbf\"", 4, "C1 overlong"},
    {"\"\xe0\x80\x80\"", 5, "E0 overlong"},
    {"\"\xed\xa0\x80\"", 5, "encoded surrogate"},
    {"\"\xf0\x80\x80\x80\"", 6, "F0 overlong"},
    {"\"\xf4\x90\x80\x80\"", 6, "above U+10FFFF"},
    {"\"\xf5\x80\x80\x80\"", 6, "F5 lead"},
    {"\"\xc2\"", 3, "truncated 2-byte"},
    {"\"\xe2\x82\"", 4, "truncated 3-byte"},
    {"\"\xf0\x9f\x8c\"", 5, "truncated 4-byte"},
    {"\"\xed\x9f\xc0\"", 5, "bad continuation"}};

static const struct bad_case BAD_ESCAPES[] = {
    {"\"\\udc00\"", 8, "lone low surrogate"},
    {"\"\\ud800\"", 8, "lone high surrogate"},
    {"\"\\ud800\\u0041\"", 14, "high + BMP escape"},
    {"\"\\ud800\\ud800\"", 14, "high + high"},
    {"\"\\ud800\\n\"", 10, "high + non-u escape"},
    {"\"\\ud800", 7, "high + end of input"},
    {"\"\\u12g4\"", 8, "bad hex escape"},
    {"\"\\ud800\\u12g4\"", 14, "high + bad hex"}};

/* Parse `text`; on success copy the stored string bytes to out and return 1. */
static int stored_bytes(const char *text, j89_len n, unsigned char *out,
                        j89_len *outlen)
{
    j89_arena a;
    j89_len root;
    const char *v;
    j89_len len;
    j89_len i;
    int ok;

    j89_arena_init(&a);
    root = j89_parse(text, n, &a);
    ok = 0;
    if (root != J89_BAD)
    {
        v = j89_string_value(&a, root);
        len = j89_string_length(&a, root);
        if (len <= 16)
        {
            for (i = 0; i < len; i = i + 1)
            {
                out[i] = (unsigned char)v[i];
            }
            *outlen = len;
            ok = 1;
        }
    }
    j89_arena_destroy(&a);
    return ok;
}

/* Parse `"payload"` and copy the stored bytes. */
static int stored_payload(const unsigned char *p, j89_len n, unsigned char *out,
                          j89_len *outlen)
{
    char buf[32];
    j89_len i;

    if (n + 2 > (j89_len)sizeof buf)
    {
        return 0;
    }
    buf[0] = '"';
    for (i = 0; i < n; i = i + 1)
    {
        buf[1 + i] = (char)p[i];
    }
    buf[1 + n] = '"';
    return stored_bytes(buf, n + 2, out, outlen);
}

/* Parse `"esc"` (esc is the escape text without surrounding quotes). */
static int stored_escape(const char *esc, j89_len n, unsigned char *out,
                         j89_len *outlen)
{
    char buf[32];
    j89_len i;

    if (n + 2 > (j89_len)sizeof buf)
    {
        return 0;
    }
    buf[0] = '"';
    for (i = 0; i < n; i = i + 1)
    {
        buf[1 + i] = esc[i];
    }
    buf[1 + n] = '"';
    return stored_bytes(buf, n + 2, out, outlen);
}

static int bytes_equal(const unsigned char *x, j89_len xn,
                       const unsigned char *y, j89_len yn)
{
    j89_len i;

    if (xn != yn)
    {
        return 0;
    }
    for (i = 0; i < xn; i = i + 1)
    {
        if (x[i] != y[i])
        {
            return 0;
        }
    }
    return 1;
}

/* Raw payload must parse and store the same bytes. */
static void check_raw(const unsigned char *bytes, j89_len n, const char *label)
{
    unsigned char got[16];
    j89_len gotlen;
    int ok;

    gotlen = 0;
    ok = stored_payload(bytes, n, got, &gotlen);
    if (!ok || !bytes_equal(got, gotlen, bytes, n))
    {
        fail(label);
    }
}

/* Escape text must parse and store exactly the expected bytes. */
static void check_esc(const char *esc, j89_len n, const unsigned char *want,
                      j89_len wantn, const char *label)
{
    unsigned char got[16];
    j89_len gotlen;
    int ok;

    gotlen = 0;
    ok = stored_escape(esc, n, got, &gotlen);
    if (!ok || !bytes_equal(got, gotlen, want, wantn))
    {
        fail(label);
    }
}

/* Raw payload and escape text must both store the expected bytes. */
static void check_equiv(const unsigned char *bytes, j89_len n, const char *esc,
                        j89_len esclen, const unsigned char *want,
                        j89_len wantn, const char *label)
{
    char rawlabel[64];
    char esclabel[64];

    sprintf(rawlabel, "%s (raw)", label);
    sprintf(esclabel, "%s (escaped)", label);
    check_raw(bytes, n, rawlabel);
    check_esc(esc, esclen, want, wantn, esclabel);
}

static void expect_bad(const char *label, const char *text, j89_len n)
{
    j89_arena a;
    j89_len root;

    j89_arena_init(&a);
    root = j89_parse(text, n, &a);
    if (root != J89_BAD)
    {
        fail(label);
    }
    j89_arena_destroy(&a);
}

/* Parse `{"payload":1}` and compare the stored key bytes with `want`. */
static void check_key_raw(const unsigned char *bytes, j89_len n,
                          const char *label)
{
    char buf[64];
    j89_arena a;
    j89_len root;
    const char *k;
    j89_len kl;
    j89_len i;
    j89_len j;

    if (n + 8 > (j89_len)sizeof buf)
    {
        fail(label);
        return;
    }
    buf[0] = '{';
    buf[1] = '"';
    for (i = 0; i < n; i = i + 1)
    {
        buf[2 + i] = (char)bytes[i];
    }
    j = 2 + n;
    buf[j] = '"';
    buf[j + 1] = ':';
    buf[j + 2] = '1';
    buf[j + 3] = '}';
    j89_arena_init(&a);
    root = j89_parse(buf, j + 4, &a);
    if (root == J89_BAD)
    {
        fail(label);
        j89_arena_destroy(&a);
        return;
    }
    k = j89_object_key(&a, root, 0);
    kl = j89_object_key_length(&a, root, 0);
    if (kl != n)
    {
        fail(label);
    }
    else
    {
        for (i = 0; i < n; i = i + 1)
        {
            if ((unsigned char)k[i] != bytes[i])
            {
                fail(label);
                break;
            }
        }
    }
    j89_arena_destroy(&a);
}

/* Parse, render, and re-parse a raw scalar string; bytes must survive. */
static void check_roundtrip(const unsigned char *bytes, j89_len n,
                            const char *label)
{
    char buf[32];
    j89_arena a;
    j89_arena out;
    j89_arena back;
    j89_len root;
    j89_len root2;
    const char *v;
    j89_len len;
    j89_len i;

    if (n + 2 > (j89_len)sizeof buf)
    {
        fail(label);
        return;
    }
    buf[0] = '"';
    for (i = 0; i < n; i = i + 1)
    {
        buf[1 + i] = (char)bytes[i];
    }
    buf[1 + n] = '"';
    j89_arena_init(&a);
    j89_arena_init(&out);
    j89_arena_init(&back);
    root = j89_parse(buf, n + 2, &a);
    if (root == J89_BAD)
    {
        fail(label);
    }
    else if (j89_render(&a, root, 1, &out) != 0)
    {
        fail(label);
    }
    else
    {
        const char *txt;

        txt = (const char *)out.mem;
        root2 = j89_parse(txt, out.off, &back);
        if (root2 == J89_BAD)
        {
            fail(label);
        }
        else
        {
            v = j89_string_value(&back, root2);
            len = j89_string_length(&back, root2);
            if (!bytes_equal((const unsigned char *)v, len, bytes, n))
            {
                fail(label);
            }
        }
    }
    j89_arena_destroy(&back);
    j89_arena_destroy(&out);
    j89_arena_destroy(&a);
}

static void test_raw_boundaries(void)
{
    check_raw(U_007F, sizeof U_007F, "raw U+007F");
    check_raw(U_0080, sizeof U_0080, "raw U+0080");
    check_raw(U_07FF, sizeof U_07FF, "raw U+07FF");
    check_raw(U_0800, sizeof U_0800, "raw U+0800");
    check_raw(U_D7FF, sizeof U_D7FF, "raw U+D7FF");
    check_raw(U_E000, sizeof U_E000, "raw U+E000");
    check_raw(U_FFFF, sizeof U_FFFF, "raw U+FFFF");
    check_raw(U_10000, sizeof U_10000, "raw U+10000");
    check_raw(U_10FFFF, sizeof U_10FFFF, "raw U+10FFFF");
    check_raw(U_1F600, sizeof U_1F600, "raw U+1F600");
}

static void test_raw_malformed(void)
{
    size_t i;

    for (i = 0; i < sizeof BAD_RAW / sizeof BAD_RAW[0]; i = i + 1)
    {
        expect_bad(BAD_RAW[i].label, BAD_RAW[i].text, BAD_RAW[i].len);
    }
}

static void test_escape_boundaries(void)
{
    check_esc(E_0000, sizeof E_0000 - 1, U_0000, sizeof U_0000,
              "escape U+0000");
    check_esc(E_007F, sizeof E_007F - 1, U_007F, sizeof U_007F,
              "escape U+007F");
    check_esc(E_0080, sizeof E_0080 - 1, U_0080, sizeof U_0080,
              "escape U+0080");
    check_esc(E_07FF, sizeof E_07FF - 1, U_07FF, sizeof U_07FF,
              "escape U+07FF");
    check_esc(E_D7FF, sizeof E_D7FF - 1, U_D7FF, sizeof U_D7FF,
              "escape U+D7FF");
    check_esc(E_E000, sizeof E_E000 - 1, U_E000, sizeof U_E000,
              "escape U+E000");
    check_esc(E_FFFF, sizeof E_FFFF - 1, U_FFFF, sizeof U_FFFF,
              "escape U+FFFF");
    check_esc(E_10000, sizeof E_10000 - 1, U_10000, sizeof U_10000,
              "pair U+10000");
    check_esc(E_103FF, sizeof E_103FF - 1, U_103FF, sizeof U_103FF,
              "pair U+103FF");
    check_esc(E_10FC00, sizeof E_10FC00 - 1, U_10FC00, sizeof U_10FC00,
              "pair U+10FC00");
    check_esc(E_10FFFF, sizeof E_10FFFF - 1, U_10FFFF, sizeof U_10FFFF,
              "pair U+10FFFF");
    check_esc(E_1F600, sizeof E_1F600 - 1, U_1F600, sizeof U_1F600,
              "pair U+1F600");
}

static void test_equivalence(void)
{
    check_equiv(U_007F, sizeof U_007F, E_007F, sizeof E_007F - 1, U_007F,
                sizeof U_007F, "U+007F");
    check_equiv(U_0080, sizeof U_0080, E_0080, sizeof E_0080 - 1, U_0080,
                sizeof U_0080, "U+0080");
    check_equiv(U_07FF, sizeof U_07FF, E_07FF, sizeof E_07FF - 1, U_07FF,
                sizeof U_07FF, "U+07FF");
    check_equiv(U_D7FF, sizeof U_D7FF, E_D7FF, sizeof E_D7FF - 1, U_D7FF,
                sizeof U_D7FF, "U+D7FF");
    check_equiv(U_E000, sizeof U_E000, E_E000, sizeof E_E000 - 1, U_E000,
                sizeof U_E000, "U+E000");
    check_equiv(U_FFFF, sizeof U_FFFF, E_FFFF, sizeof E_FFFF - 1, U_FFFF,
                sizeof U_FFFF, "U+FFFF");
    check_equiv(U_10000, sizeof U_10000, E_10000, sizeof E_10000 - 1, U_10000,
                sizeof U_10000, "U+10000");
    check_equiv(U_103FF, sizeof U_103FF, E_103FF, sizeof E_103FF - 1, U_103FF,
                sizeof U_103FF, "U+103FF");
    check_equiv(U_10FC00, sizeof U_10FC00, E_10FC00, sizeof E_10FC00 - 1,
                U_10FC00, sizeof U_10FC00, "U+10FC00");
    check_equiv(U_10FFFF, sizeof U_10FFFF, E_10FFFF, sizeof E_10FFFF - 1,
                U_10FFFF, sizeof U_10FFFF, "U+10FFFF");
    check_equiv(U_1F600, sizeof U_1F600, E_1F600, sizeof E_1F600 - 1, U_1F600,
                sizeof U_1F600, "U+1F600");
}

static void test_malformed_escapes(void)
{
    size_t i;

    for (i = 0; i < sizeof BAD_ESCAPES / sizeof BAD_ESCAPES[0]; i = i + 1)
    {
        expect_bad(BAD_ESCAPES[i].label, BAD_ESCAPES[i].text,
                   BAD_ESCAPES[i].len);
    }
}

static void test_object_keys(void)
{
    check_key_raw(U_0080, sizeof U_0080, "key raw U+0080");
    check_key_raw(U_10000, sizeof U_10000, "key raw U+10000");
    check_key_raw(U_10FFFF, sizeof U_10FFFF, "key raw U+10FFFF");
    expect_bad("key lone continuation", "{\"\x80\":1}", 6);
    expect_bad("key encoded surrogate", "{\"\xed\xa0\x80\":1}", 8);
    expect_bad("key above U+10FFFF", "{\"\xf4\x90\x80\x80\":1}", 9);
    expect_bad("key lone low surrogate", "{\"\\udc00\":1}", 11);
    expect_bad("key lone high surrogate", "{\"\\ud800\":1}", 11);
    expect_bad("key high + BMP", "{\"\\ud800\\u0041\":1}", 17);
}

static void test_render_roundtrip(void)
{
    check_roundtrip(U_007F, sizeof U_007F, "roundtrip U+007F");
    check_roundtrip(U_0080, sizeof U_0080, "roundtrip U+0080");
    check_roundtrip(U_0800, sizeof U_0800, "roundtrip U+0800");
    check_roundtrip(U_D7FF, sizeof U_D7FF, "roundtrip U+D7FF");
    check_roundtrip(U_E000, sizeof U_E000, "roundtrip U+E000");
    check_roundtrip(U_FFFF, sizeof U_FFFF, "roundtrip U+FFFF");
    check_roundtrip(U_10000, sizeof U_10000, "roundtrip U+10000");
    check_roundtrip(U_10FFFF, sizeof U_10FFFF, "roundtrip U+10FFFF");
    check_roundtrip(U_1F600, sizeof U_1F600, "roundtrip U+1F600");
}

int main(void)
{
    test_raw_boundaries();
    test_raw_malformed();
    test_escape_boundaries();
    test_equivalence();
    test_malformed_escapes();
    test_object_keys();
    test_render_roundtrip();
    if (failures != 0)
    {
        fprintf(stderr, "test_unicode: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_unicode: OK\n");
    return 0;
}
