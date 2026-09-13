/* test_sa.c - SA-IS suffix array correctness against a brute-force reference
 * over randomized and structured inputs (SA-1, SA-2 scenarios). */
#include "test.h"

#include <bwt89_internal.h>

static unsigned long rng = 88172645463325252UL;

static int rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (int)(rng % 1000000000UL);
}

/* Brute-force lexicographic comparison of suffixes a and b of s (length n,
 * sentinel n-1 unique minimum, treated as shorter-than-end = smallest). */
static int suffix_cmp(const int *s, int n, int a, int b)
{
    int i;
    for (i = 0; i < n; ++i)
    {
        int x = (a + i < n) ? s[a + i] : -1;
        int y = (b + i < n) ? s[b + i] : -1;
        if (x != y)
        {
            return x < y ? -1 : 1;
        }
    }
    return 0;
}

static void reference_sa(const int *s, int n, int *out)
{
    int i;
    int j;
    for (i = 0; i < n; ++i)
    {
        out[i] = i;
    }
    for (i = 0; i < n; ++i)
    {
        for (j = i + 1; j < n; ++j)
        {
            if (suffix_cmp(s, n, out[j], out[i]) < 0)
            {
                int tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }
}

static void check_sa(const int *s, int n, int K)
{
    int *sa;
    int *ref;
    int i;
    int r;
    sa = (int *)malloc((size_t)n * sizeof(int));
    ref = (int *)malloc((size_t)n * sizeof(int));
    if (sa == NULL || ref == NULL)
    {
        free(sa);
        free(ref);
        ++test_failures;
        return;
    }
    r = bwt89_sa_is(s, sa, n, K);
    CHECK(r == 0);
    reference_sa(s, n, ref);
    for (i = 0; i < n; ++i)
    {
        CHECK(sa[i] == ref[i]);
    }
    free(sa);
    free(ref);
}

static void random_sa(int iters, int maxn, int alpha)
{
    int it;
    int n;
    int i;
    int *s;
    for (it = 0; it < iters; ++it)
    {
        n = 1 + rnd() % maxn;
        s = (int *)malloc((size_t)(n + 1) * sizeof(int));
        if (s == NULL)
        {
            ++test_failures;
            continue;
        }
        for (i = 0; i < n; ++i)
        {
            s[i] = 1 + rnd() % alpha;
        }
        s[n] = 0;
        check_sa(s, n + 1, alpha);
        free(s);
    }
}

static void structured_sa(void)
{
    int n;
    int i;
    int *s;
    for (n = 1; n <= 60; ++n)
    {
        s = (int *)malloc((size_t)(n + 1) * sizeof(int));
        if (s == NULL)
        {
            ++test_failures;
            continue;
        }
        for (i = 0; i < n; ++i)
        {
            s[i] = 1;
        }
        s[n] = 0;
        check_sa(s, n + 1, 1);
        for (i = 0; i < n; ++i)
        {
            s[i] = 1 + (i % 2);
        }
        s[n] = 0;
        check_sa(s, n + 1, 2);
        for (i = 0; i < n; ++i)
        {
            s[i] = 1 + (i % 3);
        }
        s[n] = 0;
        check_sa(s, n + 1, 3);
        free(s);
    }
}

/* Exhaustively enumerate every text of length n (including the sentinel at
 * n-1) over symbols {1..alpha} in the earlier positions and check against the
 * brute-force reference (acceptance E02/E03). */
static void exhaustive_len(int n, int alpha)
{
    int *s;
    unsigned long pow;
    unsigned long code;
    s = (int *)malloc((size_t)n * sizeof(int));
    if (s == NULL)
    {
        ++test_failures;
        return;
    }
    pow = 1;
    {
        int i;
        for (i = 0; i < n - 1; ++i)
        {
            pow *= (unsigned long)alpha;
        }
    }
    for (code = 0; code < pow; ++code)
    {
        unsigned long c;
        int i;
        c = code;
        for (i = 0; i < n - 1; ++i)
        {
            s[i] = 1 + (int)(c % (unsigned long)alpha);
            c /= (unsigned long)alpha;
        }
        s[n - 1] = 0;
        check_sa(s, n, alpha);
    }
    free(s);
}

static void exhaustive_sa(void)
{
    int n;
    for (n = 1; n <= 13; ++n)
    {
        exhaustive_len(n, 2);
    }
    for (n = 1; n <= 9; ++n)
    {
        exhaustive_len(n, 3);
    }
}

int main(void)
{
    random_sa(2000, 40, 2);
    random_sa(2000, 50, 3);
    random_sa(1500, 80, 5);
    random_sa(300, 200, 26);
    structured_sa();
    exhaustive_sa();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
