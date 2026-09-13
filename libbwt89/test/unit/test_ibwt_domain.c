/* test_ibwt_domain.c - complete inverse-domain characterization (acceptance
 * C01, regression P04).
 *
 * A deliberately slow, allocation-free naive BWT oracle defines the forward
 * image. For a finite alphabet and length n we enumerate every source text,
 * record every valid (bwt bytes, index) pair, then enumerate every candidate
 * (b, index) in the full domain and require: valid pairs decode to the exact
 * source with BWT89_OK; invalid pairs are rejected with BWT89_BAD_DATA and
 * leave the output byte-for-byte unchanged. */
#include <stddef.h>

#include "test.h"

#include <bwt89.h>

/* --- naive regular BWT oracle (independent of libbwt89) ------------------ */

struct vec
{
    int a[17];
    int n; /* number of real symbols (length n) */
};

static void text_to_a(const unsigned char *s, size_t n, struct vec *v)
{
    size_t i;
    v->n = (int)n;
    for (i = 0; i < n; ++i)
    {
        v->a[i] = (int)s[i] + 1;
    }
    v->a[n] = 0; /* sentinel, the unique minimum */
}

static int rot_cmp(const struct vec *v, int p, int q)
{
    int N;
    int t;
    N = v->n + 1;
    for (t = 0; t < N; ++t)
    {
        int x;
        int y;
        x = v->a[(p + t) % N];
        y = v->a[(q + t) % N];
        if (x != y)
        {
            return x < y ? -1 : 1;
        }
    }
    return 0;
}

/* Naive transform: sort all rotations of the sentinel-augmented text, output
 * the last-column symbol of every row except the primary (sentinel) row, and
 * report the primary row index. */
static void naive_bwt(const unsigned char *s, size_t n, size_t *index,
                      unsigned char *b)
{
    struct vec v;
    int starts[17];
    int N;
    int lastc[17];
    int i;
    int j;
    int tmp;
    int primary;
    size_t k;
    text_to_a(s, n, &v);
    N = v.n + 1;
    for (i = 0; i < N; ++i)
    {
        starts[i] = i;
    }
    for (i = 0; i < N; ++i)
    {
        for (j = i + 1; j < N; ++j)
        {
            if (rot_cmp(&v, starts[j], starts[i]) < 0)
            {
                tmp = starts[i];
                starts[i] = starts[j];
                starts[j] = tmp;
            }
        }
    }
    primary = 0;
    for (i = 0; i < N; ++i)
    {
        lastc[i] = v.a[(starts[i] + N - 1) % N];
        if (lastc[i] == 0)
        {
            primary = i;
        }
    }
    *index = (size_t)primary;
    k = 0;
    for (i = 0; i < N; ++i)
    {
        if (lastc[i] == 0)
        {
            continue; /* the sentinel row is not part of the output */
        }
        b[k] = (unsigned char)(lastc[i] - 1);
        ++k;
    }
}

/* --- helpers -------------------------------------------------------------- */

static void fill_gold(unsigned char *g, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i)
    {
        g[i] = 0xA5;
    }
}

/* --- domain sweep --------------------------------------------------------- */

static void sweep_alphabet(const unsigned char *al, size_t an, int maxn)
{
    unsigned char valid_b[1 << 12][12];
    size_t valid_i[1 << 12];
    unsigned char text[12];
    unsigned char b[12];
    unsigned char out[12];
    unsigned char gold[12];
    size_t n;
    size_t index;
    size_t vcount;
    size_t vi;
    enum bwt89_status st;
    int t;
    unsigned long total;
    unsigned long m;
    for (n = 0; n <= (size_t)maxn; ++n)
    {
        total = 1;
        for (m = 0; m < n; ++m)
        {
            total *= an;
        }
        vcount = 0;
        /* Enumerate every source text of length n, record its forward image
         * (bwt bytes, index) from the naive oracle, and require the library
         * forward transform to agree exactly (acceptance B02/B03). */
        for (t = 0; t < (int)total; ++t)
        {
            unsigned char lb[12];
            size_t lindex;
            int x;
            x = t;
            for (m = 0; m < n; ++m)
            {
                text[m] = al[(size_t)(x % (int)an)];
                x /= (int)an;
            }
            naive_bwt(text, n, &index, b);
            if (bwt89_bwt(text, n, &lindex, lb) != BWT89_OK)
            {
                ++test_failures;
            }
            CHECK(lindex == index);
            CHECK(memcmp(lb, b, n) == 0);
            memcpy(valid_b[vcount], b, n);
            valid_i[vcount] = index;
            ++vcount;
        }
        /* Enumerate every candidate (b, index) over the full domain and check
         * membership in the forward image. */
        for (t = 0; t < (int)total; ++t)
        {
            unsigned char cand[12];
            int x;
            size_t idx;
            int is_valid;
            x = t;
            for (m = 0; m < n; ++m)
            {
                cand[m] = al[(size_t)(x % (int)an)];
                x /= (int)an;
            }
            for (idx = 0; idx <= n; ++idx)
            {
                is_valid = 0;
                for (vi = 0; vi < vcount; ++vi)
                {
                    if (valid_i[vi] == idx && memcmp(valid_b[vi], cand, n) == 0)
                    {
                        is_valid = 1;
                        break;
                    }
                }
                fill_gold(gold, n);
                memcpy(out, gold, n);
                if (is_valid)
                {
                    st = bwt89_ibwt(cand, n, idx, out);
                    CHECK(st == BWT89_OK);
                    /* out must be the (unique) source mapping to this pair. */
                    naive_bwt(out, n, &index, b);
                    CHECK(memcmp(cand, b, n) == 0 && index == idx);
                }
                else
                {
                    st = bwt89_ibwt(cand, n, idx, out);
                    CHECK(st == BWT89_BAD_DATA);
                    CHECK(memcmp(out, gold, n) == 0);
                }
            }
        }
    }
}

static void test_pathological(void)
{
    static const unsigned char one[] = {0x00};
    unsigned char out[2];
    enum bwt89_status st;
    out[0] = 0xA5;
    out[1] = 0xA5;
    st = bwt89_ibwt(one, 1, 0, out);
    CHECK(st == BWT89_BAD_DATA);
    CHECK(out[0] == 0xA5);
    CHECK(out[1] == 0xA5);
}

static void test_domain(void)
{
    static const unsigned char bin[2] = {0, 1};
    static const unsigned char ter[3] = {0, 1, 0xff};
    sweep_alphabet(bin, 2, 8);
    sweep_alphabet(ter, 3, 5);
}

int main(void)
{
    test_pathological();
    test_domain();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
