/* diff_bbwt.c - byte-exact differential test of bwt89_bbwt / bwt89_ibbwt
 * against Yuta Mori's OpenBWT BWTS / UnBWTS oracle (acceptance D, L).
 *
 * Run via `just oracle`. This program is not part of the unit suite; it links
 * the vendored OpenBWT BWTS.c (compiled separately) as an independent oracle.
 * It never shares suffix-array or transform code with libbwt89.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bwt89.h>
#include <openbwt.h>

static int failures;

static void check_match(const unsigned char *s, int n)
{
    unsigned char *lib;
    unsigned char *ob;
    unsigned char *back;
    int *A;
    int r;
    enum bwt89_status st;
    A = (int *)malloc((size_t)(6 * (n + 1) + 64) * sizeof(int));
    lib = (unsigned char *)malloc((size_t)(n ? n : 1));
    ob = (unsigned char *)malloc((size_t)(n ? n : 1));
    back = (unsigned char *)malloc((size_t)(n ? n : 1));
    if (A == NULL || lib == NULL || ob == NULL || back == NULL)
    {
        free(A);
        free(lib);
        free(ob);
        free(back);
        ++failures;
        return;
    }
    if (n == 0)
    {
        /* lib handles the empty string; openbwt BWTS is only run for n >= 1 */
        st = bwt89_bbwt(s, 0, lib);
        if (st != BWT89_OK)
        {
            ++failures;
        }
        free(A);
        free(lib);
        free(ob);
        free(back);
        return;
    }
    st = bwt89_bbwt(s, (size_t)n, lib);
    r = BWTS(s, ob, A, n);
    if (st != BWT89_OK || r != 0)
    {
        ++failures;
    }
    else if (memcmp(lib, ob, (size_t)n) != 0)
    {
        fprintf(stderr, "bbwt forward mismatch n=%d\n", n);
        ++failures;
    }
    /* inverse round trips through both implementations */
    st = bwt89_ibbwt(ob, (size_t)n, back);
    if (st != BWT89_OK || memcmp(back, s, (size_t)n) != 0)
    {
        fprintf(stderr, "ibbwt round-trip failed n=%d\n", n);
        ++failures;
    }
    free(A);
    free(lib);
    free(ob);
    free(back);
}

static void enumerate(int alpha, int maxn)
{
    unsigned char s[64];
    unsigned long pow;
    unsigned long code;
    int n;
    for (n = 1; n <= maxn; ++n)
    {
        pow = 1;
        {
            int i;
            for (i = 0; i < n; ++i)
            {
                pow *= (unsigned long)alpha;
            }
        }
        for (code = 0; code < pow; ++code)
        {
            unsigned long c;
            int i;
            c = code;
            for (i = 0; i < n; ++i)
            {
                s[i] = (unsigned char)(c % (unsigned long)alpha);
                c /= (unsigned long)alpha;
            }
            check_match(s, n);
        }
    }
}

static void random_batch(void)
{
    unsigned char s[2048];
    unsigned long seed;
    int i;
    int n;
    int it;
    seed = 0x1234ABCDUL;
    for (it = 0; it < 1500; ++it)
    {
        seed = seed * 1664525UL + 1013904223UL;
        n = 1 + (int)((seed >> 8) % 2048UL);
        for (i = 0; i < n; ++i)
        {
            seed = seed * 1664525UL + 1013904223UL;
            s[i] = (unsigned char)(seed >> 16);
        }
        check_match(s, n);
    }
}

static void fixed_vectors(void)
{
    static const char *v[] = {"",    "a",      "b",       "ab",
                              "ba",  "banana", "aab",     "aaab",
                              "abc", "BANANA", "^BANANA$"};
    size_t i;
    for (i = 0; i < sizeof v / sizeof v[0]; ++i)
    {
        check_match((const unsigned char *)v[i], (int)strlen(v[i]));
    }
}

int main(void)
{
    fixed_vectors();
    enumerate(3, 9);  /* {0,1,2} exhaustive through length 9 */
    enumerate(2, 12); /* {0,1} exhaustive through length 12 */
    random_batch();
    if (failures != 0)
    {
        fprintf(stderr, "%d oracle mismatch(es)\n", failures);
        return 1;
    }
    printf("bbwt/ibbwt match openbwt oracle + roundtrip\n");
    return 0;
}
