/* test_unicode_corpus.c - UTF-8 validity differential against libu89. */

#include "str89_test.h"

static void test_scalar_boundaries(void)
{
    static const u89_cp cps[] = {0x0000, 0x0001,  0x007F,  0x0080,
                                 0x07FF, 0x0800,  0xD7FF,  0xE000,
                                 0xFFFF, 0x10000, 0x10FFFF};
    unsigned char buf[8];
    str89_view v;
    size_t i;
    int w;
    int r;

    for (i = 0; i < sizeof(cps) / sizeof(cps[0]); i += 1)
    {
        w = u89_utf8_encode(cps[i], buf);
        str89_test_check(w > 0, "corpus: encode");
        r = str89_view_init(&v, buf, (size_t)w);
        str89_test_check_status(r, STR89_OK, "corpus: boundary accepted");
        str89_test_valid_view(v, "corpus: boundary valid");
    }
}

struct badcase
{
    const unsigned char *p;
    size_t n;
};

static void test_malformed_corpus(void)
{
    static const unsigned char b01[] = {0x80};
    static const unsigned char b02[] = {0xBF};
    static const unsigned char b03[] = {0xC0, 0x80};
    static const unsigned char b04[] = {0xC1, 0xBF};
    static const unsigned char b05[] = {0xC2};
    static const unsigned char b06[] = {0xE0, 0x80, 0x80};
    static const unsigned char b07[] = {0xE0, 0xA0};
    static const unsigned char b08[] = {0xED, 0xA0, 0x80};
    static const unsigned char b09[] = {0xED, 0xBF, 0xBF};
    static const unsigned char b10[] = {0xF0, 0x80, 0x80, 0x80};
    static const unsigned char b11[] = {0xF4, 0x90, 0x80, 0x80};
    static const unsigned char b12[] = {0xF5, 0x80, 0x80, 0x80};
    static const unsigned char b13[] = {0xFF};
    static const unsigned char b14[] = {0xFE};
    static const unsigned char b15[] = {0x41, 0x80};
    static const unsigned char b16[] = {0xC2, 0x41};
    static const struct badcase bad[] = {
        {b01, sizeof(b01)}, {b02, sizeof(b02)}, {b03, sizeof(b03)},
        {b04, sizeof(b04)}, {b05, sizeof(b05)}, {b06, sizeof(b06)},
        {b07, sizeof(b07)}, {b08, sizeof(b08)}, {b09, sizeof(b09)},
        {b10, sizeof(b10)}, {b11, sizeof(b11)}, {b12, sizeof(b12)},
        {b13, sizeof(b13)}, {b14, sizeof(b14)}, {b15, sizeof(b15)},
        {b16, sizeof(b16)}};
    str89_view v;
    size_t i;
    int r;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i += 1)
    {
        r = str89_view_init(&v, bad[i].p, bad[i].n);
        str89_test_check_status(r, STR89_EUTF8, "corpus: malformed rejected");
    }
}

static void test_random_differential(void)
{
    unsigned long st;
    unsigned char buf[16];
    str89_view v;
    size_t iter;
    size_t len;
    size_t i;
    int want;
    int r;

    st = 0x89ABCDEFUL;
    for (iter = 0; iter < 50000; iter += 1)
    {
        len = (size_t)(str89_test_rand(&st) % 9);
        for (i = 0; i < len; i += 1)
        {
            buf[i] = (unsigned char)(str89_test_rand(&st) & 0xFFUL);
        }
        want = u89_utf8_valid(buf, len);
        r = str89_view_init(&v, buf, len);
        if (want != 0)
        {
            str89_test_check_status(r, STR89_OK,
                                    "corpus: random valid accepted");
        }
        else
        {
            str89_test_check_status(r, STR89_EUTF8,
                                    "corpus: random invalid rejected");
        }
    }
}

static void test_corrupted_valid(void)
{
    unsigned long st;
    unsigned char buf[32];
    str89_view v;
    size_t iter;
    size_t len;
    size_t pos;
    u89_cp cp;
    int w;
    int want;
    int r;

    st = 0x13579BDFUL;
    for (iter = 0; iter < 20000; iter += 1)
    {
        len = 0;
        while (len + 4 <= sizeof(buf))
        {
            cp = (u89_cp)(str89_test_rand(&st) % 0x110000UL);
            w = u89_utf8_encode(cp, buf + len);
            if (w == 0)
            {
                break;
            }
            len += (size_t)w;
            if ((str89_test_rand(&st) % 4) == 0)
            {
                break;
            }
        }
        if (len == 0)
        {
            continue;
        }
        pos = (size_t)(str89_test_rand(&st) % len);
        buf[pos] = (unsigned char)(str89_test_rand(&st) & 0xFFUL);
        want = u89_utf8_valid(buf, len);
        r = str89_view_init(&v, buf, len);
        if (want != 0)
        {
            str89_test_check_status(r, STR89_OK,
                                    "corpus: corrupted valid accepted");
        }
        else
        {
            str89_test_check_status(r, STR89_EUTF8,
                                    "corpus: corrupted invalid rejected");
        }
    }
}

int main(void)
{
    test_scalar_boundaries();
    test_malformed_corpus();
    test_random_differential();
    test_corrupted_valid();
    return str89_test_report();
}
