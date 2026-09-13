/* test_replace.c - replacement of scalar-aligned ranges. */

#include "str89_test.h"

static str89_view vbytes(const unsigned char *b, size_t n)
{
    str89_view v;
    int r;

    r = str89_view_init(&v, b, n);
    str89_test_check_status(r, STR89_OK, "replace: view accepted");
    return v;
}

/* 41 E2 82 AC F0 90 8D 88 00: boundaries 0,1,4,8,9 */
static const unsigned char mixed[] = {0x41, 0xE2, 0x82, 0xAC, 0xF0,
                                      0x90, 0x8D, 0x88, 0x00};

static void replace_case(size_t offset, size_t rmlen, const unsigned char *src,
                         size_t slen, const unsigned char *want, size_t wlen,
                         const char *what)
{
    str89_buf b;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, vbytes(mixed, sizeof(mixed)));
    str89_test_check_status(r, STR89_OK, what);
    r = str89_buf_replace(&b, NULL, offset, rmlen, vbytes(src, slen));
    str89_test_check_status(r, STR89_OK, what);
    str89_test_valid_buf(&b, what);
    str89_test_view_is(str89_buf_view(&b), want, wlen, what);
    str89_buf_free(&b, NULL);
}

static void test_replace_kinds(void)
{
    static const unsigned char z[] = "Z";
    static const unsigned char ab[] = "ab";
    static const unsigned char xyz[] = "xyz";
    static const unsigned char wxyz[] = "wxyz";
    static const unsigned char euro[] = {0xE2, 0x82, 0xAC};
    static const unsigned char nul[] = {0x00};

    replace_case(1, 0, z, 1,
                 (const unsigned char *)"\x41Z\xE2\x82\xAC\xF0\x90\x8D\x88\x00",
                 10, "replace: insert-like");
    replace_case(1, 3, z, 1, (const unsigned char *)"\x41Z\xF0\x90\x8D\x88\x00",
                 7, "replace: shorter");
    replace_case(1, 3, xyz, 3,
                 (const unsigned char *)"\x41xyz\xF0\x90\x8D\x88\x00", 9,
                 "replace: same length");
    replace_case(1, 3, wxyz, 4,
                 (const unsigned char *)"\x41wxyz\xF0\x90\x8D\x88\x00", 10,
                 "replace: longer");
    replace_case(1, 3, NULL, 0,
                 (const unsigned char *)"\x41\xF0\x90\x8D\x88\x00", 6,
                 "replace: erase-like");
    replace_case(0, 9, z, 1, z, 1, "replace: whole");
    replace_case(1, 3, euro, 3,
                 (const unsigned char *)"\x41\xE2\x82\xAC\xF0\x90\x8D\x88\x00",
                 9, "replace: multibyte");
    replace_case(1, 3, nul, 1,
                 (const unsigned char *)"\x41\x00\xF0\x90\x8D\x88\x00", 7,
                 "replace: NUL");
    replace_case(1, 7, ab, 2, (const unsigned char *)"\x41\x61\x62\x00", 4,
                 "replace: many scalars");
}

static void test_replace_rejects(void)
{
    str89_buf b;
    size_t len;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, vbytes(mixed, sizeof(mixed)));
    str89_test_check_status(r, STR89_OK, "replace reject: setup");
    len = b.len;

    r = str89_buf_replace(&b, NULL, 2, 1,
                          vbytes((const unsigned char *)"Z", 1));
    str89_test_check_status(r, STR89_EBOUND, "replace: start inside");
    r = str89_buf_replace(&b, NULL, 1, 2,
                          vbytes((const unsigned char *)"Z", 1));
    str89_test_check_status(r, STR89_EBOUND, "replace: end inside");
    r = str89_buf_replace(&b, NULL, sizeof(mixed) + 1, 0,
                          vbytes((const unsigned char *)"Z", 1));
    str89_test_check_status(r, STR89_ERANGE, "replace: offset past end");
    r = str89_buf_replace(&b, NULL, 1, sizeof(mixed),
                          vbytes((const unsigned char *)"Z", 1));
    str89_test_check_status(r, STR89_ERANGE, "replace: length past end");
    r = str89_buf_replace(&b, NULL, STR89_NPOS, 1,
                          vbytes((const unsigned char *)"Z", 1));
    str89_test_check_status(r, STR89_ERANGE, "replace: SIZE_MAX offset");
    r = str89_buf_replace(&b, NULL, 1, STR89_NPOS,
                          vbytes((const unsigned char *)"Z", 1));
    str89_test_check_status(r, STR89_ERANGE, "replace: SIZE_MAX length");
    str89_test_check(b.len == len, "replace: rejection leaves len");
    str89_test_view_is(str89_buf_view(&b), mixed, sizeof(mixed),
                       "replace: rejection leaves content");
    str89_buf_free(&b, NULL);
}

int main(void)
{
    test_replace_kinds();
    test_replace_rejects();
    return str89_test_report();
}
