/* test_set.c - whole-content replacement. */

#include <string.h>

#include "str89_test.h"

static str89_view vbytes(const unsigned char *b, size_t n)
{
    str89_view v;
    int r;

    r = str89_view_init(&v, b, n);
    str89_test_check_status(r, STR89_OK, "set: view accepted");
    return v;
}

static void set_case(const unsigned char *init, size_t ilen,
                     const unsigned char *src, size_t slen,
                     const unsigned char *want, size_t wlen, const char *what)
{
    str89_buf b;
    int r;

    str89_buf_init(&b);
    if (ilen != 0)
    {
        r = str89_buf_append(&b, NULL, vbytes(init, ilen));
        str89_test_check_status(r, STR89_OK, what);
    }
    r = str89_buf_set(&b, NULL, vbytes(src, slen));
    str89_test_check_status(r, STR89_OK, what);
    str89_test_valid_buf(&b, what);
    str89_test_view_is(str89_buf_view(&b), want, wlen, what);
    str89_buf_free(&b, NULL);
}

static void test_set_kinds(void)
{
    static const unsigned char abc[] = "abc";
    static const unsigned char abcdef[] = "abcdef";
    static const unsigned char ab[] = "ab";
    static const unsigned char xyz[] = "xyz";
    static const unsigned char euro[] = {0xE2, 0x82, 0xAC};
    static const unsigned char nul[] = {'a', 0x00, 'b'};
    static const unsigned char empty[] = "";

    set_case(NULL, 0, empty, 0, empty, 0, "set: empty to empty");
    set_case(NULL, 0, abc, 3, abc, 3, "set: empty to ascii");
    set_case(abc, 3, empty, 0, empty, 0, "set: ascii to empty");
    set_case(abcdef, 6, ab, 2, ab, 2, "set: shorter");
    set_case(abc, 3, xyz, 3, xyz, 3, "set: same length");
    set_case(abc, 3, abcdef, 6, abcdef, 6, "set: longer");
    set_case(abc, 3, euro, 3, euro, 3, "set: multibyte");
    set_case(abc, 3, nul, 3, nul, 3, "set: embedded NUL");
}

static void test_set_self(void)
{
    str89_buf b;
    str89_view whole;
    str89_view prefix;
    str89_view suffix;
    str89_view interior;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, str89_test_cview("abcdef"));
    str89_test_check_status(r, STR89_OK, "set self: setup");

    whole = str89_buf_view(&b);
    r = str89_buf_set(&b, NULL, whole);
    str89_test_check_status(r, STR89_OK, "set self: whole");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"abcdef", 6,
                       "set self: whole bytes");

    prefix.data = b.data;
    prefix.len = 3;
    r = str89_buf_set(&b, NULL, prefix);
    str89_test_check_status(r, STR89_OK, "set self: prefix");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"abc", 3,
                       "set self: prefix bytes");

    r = str89_buf_set(&b, NULL, str89_test_cview("abcdef"));
    str89_test_check_status(r, STR89_OK, "set self: reset");
    suffix.data = b.data + 3;
    suffix.len = 3;
    r = str89_buf_set(&b, NULL, suffix);
    str89_test_check_status(r, STR89_OK, "set self: suffix");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"def", 3,
                       "set self: suffix bytes");

    r = str89_buf_set(&b, NULL, str89_test_cview("abcdef"));
    str89_test_check_status(r, STR89_OK, "set self: reset 2");
    interior.data = b.data + 2;
    interior.len = 2;
    r = str89_buf_set(&b, NULL, interior);
    str89_test_check_status(r, STR89_OK, "set self: interior");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"cd", 2,
                       "set self: interior bytes");

    str89_buf_free(&b, NULL);
}

int main(void)
{
    test_set_kinds();
    test_set_self();
    return str89_test_report();
}
