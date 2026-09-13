/* test_insert.c - insertion at scalar boundaries. */

#include "str89_test.h"

static str89_view vbytes(const unsigned char *b, size_t n)
{
    str89_view v;
    int r;

    r = str89_view_init(&v, b, n);
    str89_test_check_status(r, STR89_OK, "insert: view accepted");
    return v;
}

/* 41 E2 82 AC F0 90 8D 88 00: boundaries 0,1,4,8,9 */
static const unsigned char mixed[] = {0x41, 0xE2, 0x82, 0xAC, 0xF0,
                                      0x90, 0x8D, 0x88, 0x00};

static void insert_case(size_t offset, const unsigned char *src, size_t slen,
                        const unsigned char *want, size_t wlen,
                        const char *what)
{
    str89_buf b;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, vbytes(mixed, sizeof(mixed)));
    str89_test_check_status(r, STR89_OK, what);
    r = str89_buf_insert(&b, NULL, offset, vbytes(src, slen));
    str89_test_check_status(r, STR89_OK, what);
    str89_test_valid_buf(&b, what);
    str89_test_view_is(str89_buf_view(&b), want, wlen, what);
    str89_buf_free(&b, NULL);
}

static void test_insert_at_boundaries(void)
{
    static const unsigned char x[] = "XY";
    static const unsigned char nul[] = {0x00};

    insert_case(0, x, 2,
                (const unsigned char *)"XY\x41\xE2\x82\xAC\xF0\x90\x8D\x88\x00",
                11, "insert: at 0");
    insert_case(1, x, 2,
                (const unsigned char *)"\x41XY\xE2\x82\xAC\xF0\x90\x8D\x88\x00",
                11, "insert: at 1");
    insert_case(4, x, 2,
                (const unsigned char *)"\x41\xE2\x82\xACXY\xF0\x90\x8D\x88\x00",
                11, "insert: at 4");
    insert_case(8, x, 2,
                (const unsigned char *)"\x41\xE2\x82\xAC\xF0\x90\x8D\x88XY\x00",
                11, "insert: at 8");
    insert_case(9, x, 2,
                (const unsigned char *)"\x41\xE2\x82\xAC\xF0\x90\x8D\x88\x00XY",
                11, "insert: at 9");
    insert_case(
        4, nul, 1,
        (const unsigned char *)"\x41\xE2\x82\xAC\x00\xF0\x90\x8D\x88\x00", 10,
        "insert: NUL");
    insert_case(4, NULL, 0, mixed, sizeof(mixed), "insert: empty source");
}

static void test_insert_rejects(void)
{
    str89_buf b;
    size_t len;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, vbytes(mixed, sizeof(mixed)));
    str89_test_check_status(r, STR89_OK, "insert reject: setup");
    len = b.len;

    r = str89_buf_insert(&b, NULL, 2, vbytes((const unsigned char *)"Z", 1));
    str89_test_check_status(r, STR89_EBOUND, "insert: inside 3-byte scalar");
    r = str89_buf_insert(&b, NULL, 5, vbytes((const unsigned char *)"Z", 1));
    str89_test_check_status(r, STR89_EBOUND, "insert: inside 4-byte scalar");
    r = str89_buf_insert(&b, NULL, sizeof(mixed) + 1,
                         vbytes((const unsigned char *)"Z", 1));
    str89_test_check_status(r, STR89_ERANGE, "insert: offset past end");
    r = str89_buf_insert(&b, NULL, STR89_NPOS,
                         vbytes((const unsigned char *)"Z", 1));
    str89_test_check_status(r, STR89_ERANGE, "insert: SIZE_MAX offset");
    str89_test_check(b.len == len, "insert: rejection leaves len");
    str89_test_view_is(str89_buf_view(&b), mixed, sizeof(mixed),
                       "insert: rejection leaves content");
    str89_buf_free(&b, NULL);
}

int main(void)
{
    test_insert_at_boundaries();
    test_insert_rejects();
    return str89_test_report();
}
