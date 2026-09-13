/* test_cp.c - Unicode scalar insertion. */

#include <string.h>

#include "str89_test.h"

static const u89_cp valid_cps[] = {0x0000, 0x0001,  0x007F,  0x0080,
                                   0x07FF, 0x0800,  0xD7FF,  0xE000,
                                   0xFFFF, 0x10000, 0x10FFFF};

static void test_append_cp(void)
{
    unsigned char enc[4];
    str89_buf b;
    size_t i;
    int w;
    int r;

    for (i = 0; i < sizeof(valid_cps) / sizeof(valid_cps[0]); i += 1)
    {
        str89_buf_init(&b);
        w = u89_utf8_encode(valid_cps[i], enc);
        str89_test_check(w > 0, "cp: encode valid");
        r = str89_buf_append_cp(&b, NULL, valid_cps[i]);
        str89_test_check_status(r, STR89_OK, "cp: append_cp");
        str89_test_valid_buf(&b, "cp: valid");
        str89_test_view_is(str89_buf_view(&b), enc, (size_t)w, "cp: bytes");
        str89_buf_free(&b, NULL);
    }
}

static void test_insert_cp(void)
{
    unsigned char want[8];
    unsigned char enc[4];
    str89_buf b;
    size_t i;
    int w;
    int r;

    for (i = 0; i < sizeof(valid_cps) / sizeof(valid_cps[0]); i += 1)
    {
        str89_buf_init(&b);
        r = str89_buf_set(&b, NULL, str89_test_cview("AB"));
        str89_test_check_status(r, STR89_OK, "cp: insert setup");
        w = u89_utf8_encode(valid_cps[i], enc);
        want[0] = 'A';
        memcpy(want + 1, enc, (size_t)w);
        want[1 + w] = 'B';
        r = str89_buf_insert_cp(&b, NULL, 1, valid_cps[i]);
        str89_test_check_status(r, STR89_OK, "cp: insert_cp");
        str89_test_valid_buf(&b, "cp: insert valid");
        str89_test_view_is(str89_buf_view(&b), want, (size_t)w + 2,
                           "cp: insert bytes");
        str89_buf_free(&b, NULL);
    }
}

static void test_reject_cp(void)
{
    static const u89_cp bad[] = {0xD800, 0xDFFF, 0x110000, 0xFFFFFFFF};
    str89_buf b;
    size_t i;
    int r;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i += 1)
    {
        str89_buf_init(&b);
        r = str89_buf_set(&b, NULL, str89_test_cview("AB"));
        str89_test_check_status(r, STR89_OK, "cp: reject setup");
        r = str89_buf_append_cp(&b, NULL, bad[i]);
        str89_test_check_status(r, STR89_EINVAL, "cp: reject append");
        r = str89_buf_insert_cp(&b, NULL, 1, bad[i]);
        str89_test_check_status(r, STR89_EINVAL, "cp: reject insert");
        str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"AB", 2,
                           "cp: reject leaves content");
        str89_buf_free(&b, NULL);
    }
}

static void test_insert_cp_bad_offset(void)
{
    str89_buf b;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, str89_test_cview("AB"));
    str89_test_check_status(r, STR89_OK, "cp: offset setup");
    r = str89_buf_insert_cp(&b, NULL, 3, 0x41);
    str89_test_check_status(r, STR89_ERANGE, "cp: offset past end");
    r = str89_buf_insert_cp(&b, NULL, STR89_NPOS, 0x41);
    str89_test_check_status(r, STR89_ERANGE, "cp: SIZE_MAX offset");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"AB", 2,
                       "cp: offset rejection leaves content");
    str89_buf_free(&b, NULL);
}

int main(void)
{
    test_append_cp();
    test_insert_cp();
    test_reject_cp();
    test_insert_cp_bad_offset();
    return str89_test_report();
}
