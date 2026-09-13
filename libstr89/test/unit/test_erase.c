/* test_erase.c - removal of scalar-aligned ranges. */

#include "str89_test.h"

static str89_view vbytes(const unsigned char *b, size_t n)
{
    str89_view v;
    int r;

    r = str89_view_init(&v, b, n);
    str89_test_check_status(r, STR89_OK, "erase: view accepted");
    return v;
}

/* 41 E2 82 AC F0 90 8D 88 00: boundaries 0,1,4,8,9 */
static const unsigned char mixed[] = {0x41, 0xE2, 0x82, 0xAC, 0xF0,
                                      0x90, 0x8D, 0x88, 0x00};

static void erase_case(size_t offset, size_t len, const unsigned char *want,
                       size_t wlen, const char *what)
{
    str89_buf b;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, vbytes(mixed, sizeof(mixed)));
    str89_test_check_status(r, STR89_OK, what);
    r = str89_buf_erase(&b, offset, len);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_valid_buf(&b, what);
    str89_test_view_is(str89_buf_view(&b), want, wlen, what);
    str89_buf_free(&b, NULL);
}

static void test_erase_shapes(void)
{
    static const unsigned char first[] = {0xE2, 0x82, 0xAC, 0xF0,
                                          0x90, 0x8D, 0x88, 0x00};
    static const unsigned char euro[] = {0x41, 0xF0, 0x90, 0x8D, 0x88, 0x00};
    static const unsigned char last[] = {0x41, 0xE2, 0x82, 0xAC,
                                         0xF0, 0x90, 0x8D, 0x88};
    static const unsigned char middle[] = {0x41, 0x00};
    static const unsigned char prefix[] = {0xF0, 0x90, 0x8D, 0x88, 0x00};
    static const unsigned char suffix[] = {0x41, 0xE2, 0x82, 0xAC};
    static const unsigned char interior[] = {0x41, 0xE2, 0x82, 0xAC, 0x00};
    static const unsigned char whole[] = "";

    erase_case(0, 0, mixed, sizeof(mixed), "erase: zero at start");
    erase_case(1, 0, mixed, sizeof(mixed), "erase: zero at scalar start");
    erase_case(9, 0, mixed, sizeof(mixed), "erase: zero at end");
    erase_case(0, 1, first, sizeof(first), "erase: first scalar");
    erase_case(1, 3, euro, sizeof(euro), "erase: 3-byte scalar");
    erase_case(8, 1, last, sizeof(last), "erase: last scalar");
    erase_case(1, 7, middle, sizeof(middle), "erase: many scalars");
    erase_case(0, 4, prefix, sizeof(prefix), "erase: prefix");
    erase_case(4, 5, suffix, sizeof(suffix), "erase: suffix");
    erase_case(4, 4, interior, sizeof(interior), "erase: interior");
    erase_case(0, 9, whole, 0, "erase: whole string");
}

static void test_erase_rejects(void)
{
    str89_buf b;
    size_t len;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, vbytes(mixed, sizeof(mixed)));
    str89_test_check_status(r, STR89_OK, "erase reject: setup");
    len = b.len;

    r = str89_buf_erase(&b, 2, 1);
    str89_test_check_status(r, STR89_EBOUND, "erase: start inside scalar");
    r = str89_buf_erase(&b, 2, 0);
    str89_test_check_status(r, STR89_EBOUND, "erase: zero inside scalar");
    r = str89_buf_erase(&b, 1, 2);
    str89_test_check_status(r, STR89_EBOUND, "erase: end inside scalar");
    r = str89_buf_erase(&b, 0, 2);
    str89_test_check_status(r, STR89_EBOUND, "erase: span splits scalar");
    r = str89_buf_erase(&b, sizeof(mixed) + 1, 0);
    str89_test_check_status(r, STR89_ERANGE, "erase: offset past end");
    r = str89_buf_erase(&b, 1, sizeof(mixed));
    str89_test_check_status(r, STR89_ERANGE, "erase: length past end");
    r = str89_buf_erase(&b, STR89_NPOS, 1);
    str89_test_check_status(r, STR89_ERANGE, "erase: SIZE_MAX offset");
    r = str89_buf_erase(&b, 1, STR89_NPOS);
    str89_test_check_status(r, STR89_ERANGE, "erase: SIZE_MAX length");
    str89_test_check(b.len == len, "erase: rejection leaves len");
    str89_test_view_is(str89_buf_view(&b), mixed, sizeof(mixed),
                       "erase: rejection leaves content");
    str89_buf_free(&b, NULL);
}

int main(void)
{
    test_erase_shapes();
    test_erase_rejects();
    return str89_test_report();
}
