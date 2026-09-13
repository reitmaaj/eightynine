/* test_bbwt.c - bijective bwt/ibbwt: round trips over arbitrary data, the
 * permutation property, and a reference vector (BBWT-1..BBWT-4, A3, A4). */
#include "test.h"

static unsigned long rng = 2862933555777941757UL;

static int rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (int)(rng % 1000000000UL);
}

static void round_trip(const unsigned char *s, size_t n)
{
    unsigned char *b;
    unsigned char *r;
    size_t i;
    enum bwt89_status st;
    b = (unsigned char *)malloc(n ? n : 1);
    r = (unsigned char *)malloc(n ? n : 1);
    if (b == NULL || r == NULL)
    {
        free(b);
        free(r);
        ++test_failures;
        return;
    }
    st = bwt89_bbwt(s, n, b);
    CHECK(st == BWT89_OK);
    st = bwt89_ibbwt(b, n, r);
    CHECK(st == BWT89_OK);
    CHECK(memcmp(s, r, n) == 0);
    /* permutation: multiset of output equals multiset of input */
    for (i = 0; i < n; ++i)
    {
        size_t x;
        size_t y;
        size_t j;
        x = 0;
        y = 0;
        for (j = 0; j < n; ++j)
        {
            if (s[j] == s[i])
            {
                ++x;
            }
            if (b[j] == s[i])
            {
                ++y;
            }
        }
        CHECK(x == y);
    }
    free(b);
    free(r);
}

int main(void)
{
    unsigned char buf[160];
    unsigned char out[8];
    int it;
    size_t n;
    size_t i;
    enum bwt89_status st;
    buf[0] = 0;
    round_trip(buf, 0);
    buf[0] = 'a';
    round_trip(buf, 1);
    memcpy(buf, "banana", 6);
    round_trip(buf, 6);
    memcpy(buf, "^BANANA$", 8);
    round_trip(buf, 8);
    /* reference vector (openbwt canonical): banana -> "annbaa" */
    st = bwt89_bbwt((const unsigned char *)"banana", 6, out);
    CHECK(st == BWT89_OK);
    CHECK(memcmp(out, "annbaa", 6) == 0);
    for (it = 0; it < 4000; ++it)
    {
        n = 1 + (size_t)(rnd() % 150);
        for (i = 0; i < n; ++i)
        {
            buf[i] = (unsigned char)(rnd() % 256);
        }
        round_trip(buf, n);
    }
    for (it = 0; it < 1000; ++it)
    {
        n = 1 + (size_t)(rnd() % 80);
        for (i = 0; i < n; ++i)
        {
            buf[i] = (unsigned char)('a' + (rnd() % 3));
        }
        round_trip(buf, n);
    }
    st = bwt89_bbwt(NULL, 3, out);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ibbwt(NULL, 3, out);
    CHECK(st == BWT89_NULL_ARG);
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
