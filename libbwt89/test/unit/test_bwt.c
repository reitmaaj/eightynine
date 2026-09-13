/* test_bwt.c - regular bwt/ibwt correctness: round trips over arbitrary and
 * periodic inputs, null/bad-index rejection, and permutation preservation
 * (BWT-1..BWT-3, ERR-1, ERR-2, A1..A3, A7, A8). */
#include "test.h"

static unsigned long rng = 19073486328125UL;

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
    size_t index;
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
    st = bwt89_bwt(s, n, &index, b);
    CHECK(st == BWT89_OK);
    CHECK(index < n + 1);
    st = bwt89_ibwt(b, n, index, r);
    CHECK(st == BWT89_OK);
    CHECK(memcmp(s, r, n) == 0);
    /* permutation: transformed multiset equals input multiset */
    for (i = 0; i < n; ++i)
    {
        size_t x;
        size_t y;
        x = 0;
        y = 0;
        {
            size_t j;
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
        }
        CHECK(x == y);
    }
    free(b);
    free(r);
}

int main(void)
{
    unsigned char buf[160];
    int it;
    size_t n;
    size_t i;
    size_t index;
    unsigned char out[4];
    enum bwt89_status st;
    buf[0] = 0;
    round_trip(buf, 0); /* empty (buf contents irrelevant) */
    buf[0] = 'A';
    round_trip(buf, 1);
    memcpy(buf, "banana", 6);
    round_trip(buf, 6);
    memcpy(buf, "BANANA", 6);
    round_trip(buf, 6);
    memcpy(buf, "^BANANA$", 8);
    round_trip(buf, 8);
    for (it = 0; it < 4000; ++it)
    {
        n = 1 + (size_t)(rnd() % 150);
        for (i = 0; i < n; ++i)
        {
            buf[i] = (unsigned char)(rnd() % 256);
        }
        round_trip(buf, n);
    }
    for (it = 0; it < 800; ++it)
    {
        n = 1 + (size_t)(rnd() % 70);
        for (i = 0; i < n; ++i)
        {
            buf[i] = (unsigned char)(1 + (rnd() % 3));
        }
        round_trip(buf, n);
    }
    /* error handling */
    st = bwt89_bwt(NULL, 3, &index, out);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_bwt((const unsigned char *)"abc", 3, NULL, out);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_bwt((const unsigned char *)"abc", 3, &index, NULL);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ibwt(NULL, 3, 0, out);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ibwt((const unsigned char *)"abc", 3, 0, NULL);
    CHECK(st == BWT89_NULL_ARG);
    /* bad index out of [0, n+1) */
    st = bwt89_ibwt((const unsigned char *)"abc", 3, 3, out);
    CHECK(st == BWT89_OK); /* index up to n inclusive is the sentinel row */
    st = bwt89_ibwt((const unsigned char *)"abc", 3, 4, out);
    CHECK(st == BWT89_BAD_INDEX);
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
