/* test_alias.c - exact in-place (out == in) operation for all four static
 * transforms (acceptance F, P05, P06) and guard-byte preservation. */
#include <stddef.h>

#include "test.h"

#include <bwt89.h>

#define G 32 /* guard size */

static unsigned char guard[G];
static unsigned char buf[G + 256 + G];

static void fill_guards(void)
{
    size_t i;
    for (i = 0; i < G; ++i)
    {
        guard[i] = (unsigned char)0xA5;
        buf[i] = (unsigned char)0xA5;
        buf[G + 256 + i] = (unsigned char)0xA5;
    }
}

static int guards_ok(void)
{
    size_t i;
    for (i = 0; i < G; ++i)
    {
        if (guard[i] != buf[i])
        {
            return 0;
        }
        if (guard[i] != buf[G + 256 + i])
        {
            return 0;
        }
    }
    return 1;
}

/* disjoint out pointer lives at buf+G (payload region aliases for in-place). */
static unsigned char *payload(void)
{
    return buf + G;
}

static int gen(unsigned char *dst, unsigned long t, int n)
{
    int i;
    for (i = 0; i < n; ++i)
    {
        dst[i] = (unsigned char)(t & 1UL);
        t >>= 1;
    }
    return n;
}

static void test_regular_inplace(void)
{
    unsigned char *in;
    unsigned char text[256];
    unsigned char dout[256];
    size_t index_ip;
    size_t index_dj;
    int n;
    unsigned long t;
    unsigned long total;
    enum bwt89_status st;
    for (n = 1; n <= 12; ++n)
    {
        total = 1UL << (unsigned)n;
        for (t = 0; t < total; ++t)
        {
            gen(text, t, n);
            fill_guards();
            in = payload();

            /* in-place forward */
            memcpy(in, text, (size_t)n);
            st = bwt89_bwt(in, (size_t)n, &index_ip, in);
            CHECK(st == BWT89_OK);
            CHECK(guards_ok());

            /* disjoint forward from the same text must match exactly */
            st = bwt89_bwt(text, (size_t)n, &index_dj, dout);
            CHECK(st == BWT89_OK);
            CHECK(index_ip == index_dj);
            CHECK(memcmp(in, dout, (size_t)n) == 0);

            /* in-place inverse of the transform recovers the text */
            st = bwt89_ibwt(in, (size_t)n, index_ip, in);
            CHECK(st == BWT89_OK);
            CHECK(memcmp(in, text, (size_t)n) == 0);
        }
    }
}

static void test_bijective_inplace(void)
{
    static const unsigned char banana[] = {'b', 'a', 'n', 'a', 'n', 'a'};
    unsigned char *in;
    unsigned char text[256];
    unsigned char dout[256];
    int n;
    unsigned long t;
    unsigned long total;
    enum bwt89_status st;
    for (n = 1; n <= 12; ++n)
    {
        total = 1UL << (unsigned)n;
        for (t = 0; t < total; ++t)
        {
            gen(text, t, n);
            fill_guards();
            in = payload();

            /* disjoint forward */
            st = bwt89_bbwt(text, (size_t)n, dout);
            CHECK(st == BWT89_OK);
            CHECK(guards_ok());

            /* in-place forward must match the disjoint result */
            memcpy(in, text, (size_t)n);
            st = bwt89_bbwt(in, (size_t)n, in);
            CHECK(st == BWT89_OK);
            CHECK(memcmp(in, dout, (size_t)n) == 0);

            /* inverse in place recovers the original text */
            st = bwt89_ibbwt(in, (size_t)n, in);
            CHECK(st == BWT89_OK);
            CHECK(memcmp(in, text, (size_t)n) == 0);
            CHECK(guards_ok());
        }
    }

    /* P05/P06 fixed vector */
    in = payload();
    memcpy(in, banana, sizeof banana);
    st = bwt89_bbwt(in, sizeof banana, in);
    CHECK(st == BWT89_OK);
    CHECK(memcmp(in, "annbaa", sizeof banana) == 0);
    st = bwt89_ibbwt(in, sizeof banana, in);
    CHECK(st == BWT89_OK);
    CHECK(memcmp(in, banana, sizeof banana) == 0);
}

static void test_zero_length(void)
{
    unsigned char *p;
    size_t index;
    fill_guards();
    p = payload();
    index = 123;
    CHECK(bwt89_bwt(p, 0, &index, p) == BWT89_OK);
    CHECK(index == 0);
    CHECK(bwt89_bbwt(p, 0, p) == BWT89_OK);
    CHECK(bwt89_ibwt(p, 0, 0, p) == BWT89_OK);
    CHECK(bwt89_ibbwt(p, 0, p) == BWT89_OK);
    CHECK(guards_ok());
}

int main(void)
{
    fill_guards();
    test_regular_inplace();
    test_bijective_inplace();
    test_zero_length();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
