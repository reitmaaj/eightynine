/* test_append.c - concatenation. */

#include <string.h>

#include "str89_test.h"

static str89_view vbytes(const unsigned char *b, size_t n)
{
    str89_view v;
    int r;

    r = str89_view_init(&v, b, n);
    str89_test_check_status(r, STR89_OK, "append: view accepted");
    return v;
}

static void append_case(const unsigned char *init, size_t ilen,
                        const unsigned char *src, size_t slen,
                        const unsigned char *want, size_t wlen,
                        const char *what)
{
    str89_buf b;
    int r;

    str89_buf_init(&b);
    if (ilen != 0)
    {
        r = str89_buf_append(&b, NULL, vbytes(init, ilen));
        str89_test_check_status(r, STR89_OK, what);
    }
    r = str89_buf_append(&b, NULL, vbytes(src, slen));
    str89_test_check_status(r, STR89_OK, what);
    str89_test_valid_buf(&b, what);
    str89_test_view_is(str89_buf_view(&b), want, wlen, what);
    str89_buf_free(&b, NULL);
}

static void test_append_kinds(void)
{
    static const unsigned char ab[] = "ab";
    static const unsigned char cd[] = "cd";
    static const unsigned char two[] = {0xC3, 0xA9};
    static const unsigned char three[] = {0xE2, 0x82, 0xAC};
    static const unsigned char four[] = {0xF0, 0x90, 0x8D, 0x88};
    static const unsigned char mixed[] = {0x41, 0x00, 0xE2, 0x82, 0xAC};
    static const unsigned char nul[] = {'x', 0x00, 'y'};
    static const unsigned char empty[] = "";
    static const unsigned char abcd[] = "abcd";
    static const unsigned char abtwo[] = {0x61, 0x62, 0xC3, 0xA9};
    static const unsigned char abthree[] = {0x61, 0x62, 0xE2, 0x82, 0xAC};
    static const unsigned char abfour[] = {0x61, 0x62, 0xF0, 0x90, 0x8D, 0x88};
    static const unsigned char abnul[] = {0x61, 0x62, 0x78, 0x00, 0x79};
    static const unsigned char mixedcat[] = {0x41, 0x00, 0xE2, 0x82,
                                             0xAC, 0x78, 0x00, 0x79};
    static const unsigned char abcdmixed[] = {0x61, 0x62, 0x63, 0x64, 0x41,
                                              0x00, 0xE2, 0x82, 0xAC};

    append_case(NULL, 0, empty, 0, empty, 0, "append: empty + empty");
    append_case(NULL, 0, ab, 2, ab, 2, "append: empty + ascii");
    append_case(ab, 2, empty, 0, ab, 2, "append: ascii + empty");
    append_case(ab, 2, cd, 2, abcd, 4, "append: ascii + ascii");
    append_case(ab, 2, two, 2, abtwo, 4, "append: 2-byte");
    append_case(ab, 2, three, 3, abthree, 5, "append: 3-byte");
    append_case(ab, 2, four, 4, abfour, 6, "append: 4-byte");
    append_case(ab, 2, nul, 3, abnul, 5, "append: embedded NUL");
    append_case(mixed, 5, nul, 3, mixedcat, 8, "append: mixed + NUL");
    append_case(abcd, 4, mixed, 5, abcdmixed, 9, "append: ascii + mixed");
}

static void test_append_realloc(void)
{
    unsigned char fill[64];
    unsigned char want[128];
    str89_buf b;
    size_t need;
    size_t i;
    size_t n;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, str89_test_cview("abc"));
    str89_test_check_status(r, STR89_OK, "append realloc: setup");
    need = b.cap - b.len;
    for (i = 0; i < need; i += 1)
    {
        fill[i] = (unsigned char)('0' + (i % 10));
    }
    r = str89_buf_append(&b, NULL, vbytes(fill, need));
    str89_test_check_status(r, STR89_OK, "append realloc: fill");
    str89_test_check(b.len == b.cap, "append realloc: at capacity");
    memcpy(want, b.data, b.len);
    n = b.len;
    memcpy(want + n, b.data, b.len);
    r = str89_buf_append(&b, NULL, str89_buf_view(&b));
    str89_test_check_status(r, STR89_OK, "append realloc: self");
    str89_test_valid_buf(&b, "append realloc: valid");
    str89_test_view_is(str89_buf_view(&b), want, n * 2,
                       "append realloc: bytes");
    str89_buf_free(&b, NULL);
}

int main(void)
{
    test_append_kinds();
    test_append_realloc();
    return str89_test_report();
}
