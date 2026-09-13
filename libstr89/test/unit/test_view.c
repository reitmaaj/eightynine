/* test_view.c - str89_view_init and view accessors. */

#include "str89_test.h"

static void test_init_empty(void)
{
    str89_view v;
    int r;

    v.data = (const unsigned char *)"sentinel";
    v.len = 7;
    r = str89_view_init(&v, NULL, 0);
    str89_test_check_status(r, STR89_OK, "init: NULL/0 accepted");
    str89_test_check(v.data == NULL, "init: NULL/0 data");
    str89_test_check(v.len == 0, "init: NULL/0 len");
}

static void test_init_null_nonzero(void)
{
    str89_view v;
    int r;

    v.data = (const unsigned char *)"sentinel";
    v.len = 7;
    r = str89_view_init(&v, NULL, 1);
    str89_test_check_status(r, STR89_EINVAL, "init: NULL/1 rejected");
    str89_test_check(v.len == 7, "init: NULL/1 out unchanged");
}

static void test_init_valid(void)
{
    unsigned char buf[64];
    size_t n;
    str89_view v;
    int r;

    n = 0;
    n = str89_test_put_cp(buf, n, 0x0000);
    n = str89_test_put_cp(buf, n, 0x0041);
    n = str89_test_put_cp(buf, n, 0x007F);
    n = str89_test_put_cp(buf, n, 0x0080);
    n = str89_test_put_cp(buf, n, 0x07FF);
    n = str89_test_put_cp(buf, n, 0x0800);
    n = str89_test_put_cp(buf, n, 0xD7FF);
    n = str89_test_put_cp(buf, n, 0xE000);
    n = str89_test_put_cp(buf, n, 0xFFFF);
    n = str89_test_put_cp(buf, n, 0x10000);
    n = str89_test_put_cp(buf, n, 0x10FFFF);
    r = str89_view_init(&v, buf, n);
    str89_test_check_status(r, STR89_OK, "init: scalar boundaries accepted");
    str89_test_valid_view(v, "init: valid view is valid");
    str89_test_check(v.data == buf, "init: view borrows input");
    str89_test_check(v.len == n, "init: view length");
}

static void check_reject(const unsigned char *bytes, size_t len,
                         const char *what)
{
    str89_view v;
    int r;

    v.data = (const unsigned char *)"sentinel";
    v.len = 7;
    r = str89_view_init(&v, bytes, len);
    str89_test_check_status(r, STR89_EUTF8, what);
    str89_test_check(v.len == 7, "init: rejection leaves out unchanged");
}

static void test_init_invalid(void)
{
    static const unsigned char lone_cont[] = {0x80};
    static const unsigned char lone_cont2[] = {0xBF};
    static const unsigned char overlong_nul[] = {0xC0, 0x80};
    static const unsigned char overlong[] = {0xC1, 0xBF};
    static const unsigned char trunc2[] = {0xC2};
    static const unsigned char overlong3[] = {0xE0, 0x80, 0x80};
    static const unsigned char trunc3[] = {0xE0, 0xA0};
    static const unsigned char high_surrogate[] = {0xED, 0xA0, 0x80};
    static const unsigned char low_surrogate[] = {0xED, 0xBF, 0xBF};
    static const unsigned char overlong4[] = {0xF0, 0x80, 0x80, 0x80};
    static const unsigned char above_max[] = {0xF4, 0x90, 0x80, 0x80};
    static const unsigned char f5[] = {0xF5, 0x80, 0x80, 0x80};
    static const unsigned char ff[] = {0xFF};
    static const unsigned char fe[] = {0xFE};
    static const unsigned char good_bad[] = {0x41, 0x80};
    static const unsigned char bad_good[] = {0xC2, 0x41};

    check_reject(lone_cont, sizeof(lone_cont), "init: lone continuation 80");
    check_reject(lone_cont2, sizeof(lone_cont2), "init: lone continuation BF");
    check_reject(overlong_nul, sizeof(overlong_nul), "init: overlong NUL");
    check_reject(overlong, sizeof(overlong), "init: overlong C1 BF");
    check_reject(trunc2, sizeof(trunc2), "init: truncated 2-byte");
    check_reject(overlong3, sizeof(overlong3), "init: overlong 3-byte");
    check_reject(trunc3, sizeof(trunc3), "init: truncated 3-byte");
    check_reject(high_surrogate, sizeof(high_surrogate),
                 "init: U+D800 surrogate");
    check_reject(low_surrogate, sizeof(low_surrogate),
                 "init: U+DFFF surrogate");
    check_reject(overlong4, sizeof(overlong4), "init: overlong 4-byte");
    check_reject(above_max, sizeof(above_max), "init: above U+10FFFF");
    check_reject(f5, sizeof(f5), "init: F5 lead");
    check_reject(ff, sizeof(ff), "init: FF byte");
    check_reject(fe, sizeof(fe), "init: FE byte");
    check_reject(good_bad, sizeof(good_bad), "init: good prefix, bad suffix");
    check_reject(bad_good, sizeof(bad_good), "init: bad prefix, good suffix");
}

static void test_view_of(void)
{
    str89 s;
    str89_buf b;
    str89_view v;

    str89_init(&s);
    v = str89_view_of(&s);
    str89_test_check(v.data == NULL, "view_of: empty data");
    str89_test_check(v.len == 0, "view_of: empty len");

    str89_buf_init(&b);
    v = str89_buf_view(&b);
    str89_test_check(v.data == NULL, "buf_view: empty data");
    str89_test_check(v.len == 0, "buf_view: empty len");
}

int main(void)
{
    test_init_empty();
    test_init_null_nonzero();
    test_init_valid();
    test_init_invalid();
    test_view_of();
    return str89_test_report();
}
