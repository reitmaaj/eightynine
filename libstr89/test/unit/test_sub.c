/* test_sub.c - subview construction. */

#include "str89_test.h"

static const unsigned char mixed[] = {0x41, 0xE2, 0x82, 0xAC, 0xF0,
                                      0x90, 0x8D, 0x88, 0x00};

static str89_view mixed_view(void)
{
    str89_view v;
    int r;

    r = str89_view_init(&v, mixed, sizeof(mixed));
    str89_test_check_status(r, STR89_OK, "sub: view accepted");
    return v;
}

static void test_sub_valid(void)
{
    str89_view s;
    str89_view out;
    int r;

    s = mixed_view();
    out.data = NULL;
    out.len = 0;
    r = str89_view_sub(s, 1, 3, &out);
    str89_test_check_status(r, STR89_OK, "sub: euro scalar");
    str89_test_check(out.data == s.data + 1, "sub: points into original");
    str89_test_check(out.len == 3, "sub: length");
    str89_test_view_is(out, mixed + 1, 3, "sub: bytes");
    str89_test_valid_view(out, "sub: valid");

    r = str89_view_sub(s, 4, 4, &out);
    str89_test_check_status(r, STR89_OK, "sub: 4-byte scalar");
    str89_test_view_is(out, mixed + 4, 4, "sub: 4-byte bytes");

    r = str89_view_sub(s, 0, sizeof(mixed), &out);
    str89_test_check_status(r, STR89_OK, "sub: whole string");
    str89_test_view_is(out, mixed, sizeof(mixed), "sub: whole bytes");
}

static void test_sub_zero_length(void)
{
    str89_view s;
    str89_view out;
    int r;

    s = mixed_view();
    r = str89_view_sub(s, 1, 0, &out);
    str89_test_check_status(r, STR89_OK, "sub: zero at boundary");
    str89_test_check(out.len == 0, "sub: zero length");
    r = str89_view_sub(s, sizeof(mixed), 0, &out);
    str89_test_check_status(r, STR89_OK, "sub: empty suffix");
    r = str89_view_sub(s, 2, 0, &out);
    str89_test_check_status(r, STR89_EBOUND, "sub: zero inside scalar");
}

static void test_sub_bad_bounds(void)
{
    str89_view s;
    str89_view out;
    int r;

    s = mixed_view();
    out.data = (const unsigned char *)"sentinel";
    out.len = 42;
    r = str89_view_sub(s, 2, 1, &out);
    str89_test_check_status(r, STR89_EBOUND, "sub: split start");
    str89_test_check(out.len == 42, "sub: split start out unchanged");
    r = str89_view_sub(s, 1, 1, &out);
    str89_test_check_status(r, STR89_EBOUND, "sub: split end");
    r = str89_view_sub(s, sizeof(mixed) + 1, 0, &out);
    str89_test_check_status(r, STR89_ERANGE, "sub: offset past end");
    r = str89_view_sub(s, 1, sizeof(mixed), &out);
    str89_test_check_status(r, STR89_ERANGE, "sub: length past remainder");
    r = str89_view_sub(s, STR89_NPOS, 1, &out);
    str89_test_check_status(r, STR89_ERANGE, "sub: SIZE_MAX offset");
    r = str89_view_sub(s, 1, STR89_NPOS, &out);
    str89_test_check_status(r, STR89_ERANGE, "sub: SIZE_MAX length");
}

static void test_sub_empty_source(void)
{
    str89_view s;
    str89_view out;
    int r;

    r = str89_view_init(&s, NULL, 0);
    str89_test_check_status(r, STR89_OK, "sub: empty view");
    r = str89_view_sub(s, 0, 0, &out);
    str89_test_check_status(r, STR89_OK, "sub: empty whole");
    str89_test_check(out.data == NULL, "sub: empty data");
    str89_test_check(out.len == 0, "sub: empty len");
    r = str89_view_sub(s, 1, 0, &out);
    str89_test_check_status(r, STR89_ERANGE, "sub: empty offset 1");
}

int main(void)
{
    test_sub_valid();
    test_sub_zero_length();
    test_sub_bad_bounds();
    test_sub_empty_source();
    return str89_test_report();
}
