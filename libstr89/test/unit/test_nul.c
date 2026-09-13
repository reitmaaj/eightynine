/* test_nul.c - embedded U+0000 regression suite. */

#include <string.h>

#include "str89_test.h"

static const unsigned char n1[] = {0x00};
static const unsigned char n2[] = {'a', 0x00, 'b'};
static const unsigned char n3[] = {0x00, 'a'};
static const unsigned char n4[] = {'a', 0x00};
static const unsigned char n5[] = {0x00, 0x00};
static const unsigned char n6[] = {0xE2, 0x82, 0xAC, 0x00,
                                   0xF0, 0x90, 0x8D, 0x88};

static void exercise(const unsigned char *bytes, size_t len, const char *what)
{
    unsigned char want[64];
    str89_view v;
    str89 s;
    str89 s2;
    str89 taken;
    str89_buf b;
    size_t at;
    int r;

    r = str89_view_init(&v, bytes, len);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_valid_view(v, what);

    str89_init(&s);
    r = str89_from_view(&s, NULL, v);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_check(str89_view_equal(str89_view_of(&s), v) != 0, what);

    str89_init(&s2);
    r = str89_copy(&s2, NULL, &s);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_check(str89_view_equal(str89_view_of(&s2), v) != 0, what);

    r = str89_view_find(v, v, 0, &at);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_check(at == 0, what);
    str89_test_check(str89_view_compare(v, v) == 0, what);

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, v);
    str89_test_check_status(r, STR89_OK, what);
    r = str89_buf_append(&b, NULL, v);
    str89_test_check_status(r, STR89_OK, what);
    memcpy(want, bytes, len);
    memcpy(want + len, bytes, len);
    str89_test_view_is(str89_buf_view(&b), want, len * 2, what);
    str89_test_valid_buf(&b, what);

    r = str89_buf_erase(&b, 0, len);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_view_is(str89_buf_view(&b), want + len, len, what);

    str89_init(&taken);
    r = str89_take(&taken, &b);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_view_is(str89_view_of(&taken), want + len, len, what);
    str89_test_valid_str(&taken, what);

    str89_free(&s, NULL);
    str89_free(&s2, NULL);
    str89_free(&taken, NULL);
    str89_buf_free(&b, NULL);
}

int main(void)
{
    exercise(n1, sizeof(n1), "nul: single");
    exercise(n2, sizeof(n2), "nul: middle");
    exercise(n3, sizeof(n3), "nul: first");
    exercise(n4, sizeof(n4), "nul: last");
    exercise(n5, sizeof(n5), "nul: double");
    exercise(n6, sizeof(n6), "nul: multibyte");
    return str89_test_report();
}
