/* bwt89_bwts.c - bijective Burrows-Wheeler transform (bbwt) and inverse.
 *
 * bbwt: factor the input into its non-increasing Lyndon-word sequence
 * (Duval), sort every conjugate of every factor by the lexicographic order
 * of its infinite periodic string, and emit the last byte of each conjugate.
 * Equal conjugates share a last byte, so the ordering is deterministic.
 * ibbwt: follow the LF cycles to recover each factor and write them out in
 * reverse order.
 *
 * Byte-for-byte validated against Yuta Mori's OpenBWT BWTS oracle. Written to
 * the green worker/controller discipline: workers compute at their own top
 * level; decision/iteration bodies delegate in single glue statements.
 *
 * bbwt: O(n log n) conjugate comparisons (each bounded by the sum of two
 * factor lengths); ibbwt: O(n). */

#include <bwt89.h>

#include <string.h>

#include "bwt89_internal.h"

enum
{
    BWT89_ALPHABET = 256
};

/* --- forward declarations (all static workers) ------------------------- */

static void factorize(const unsigned char *s, int n, int *ws, int *ln,
                      int *starts);
static void sort_positions(const unsigned char *s, const int *ws, const int *ln,
                           int *ord, int *tmp, int n);
static void run_cycle(const unsigned char *b, unsigned char *out, int *rank,
                      int *wi, int start);
static void run_cycles(const unsigned char *b, unsigned char *out, int *rank,
                       int n);

/* --- pure value workers ------------------------------------------------ */

static unsigned char conj_byte(const unsigned char *s, const int *ws,
                               const int *ln, int p, int d)
{
    int a;
    int off;
    a = ws[p];
    off = ((p - a) + d) % ln[p];
    return s[a + off];
}

static int diff_at(const unsigned char *s, const int *ws, const int *ln, int p,
                   int q, int d)
{
    unsigned char x;
    unsigned char y;
    x = conj_byte(s, ws, ln, p, d);
    y = conj_byte(s, ws, ln, q, d);
    if (x < y)
    {
        return -1;
    }
    if (x > y)
    {
        return 1;
    }
    return 0;
}

/* Compare the infinite periodic strings of the conjugates at p and q. */
static int conj_cmp(const unsigned char *s, const int *ws, const int *ln, int p,
                    int q)
{
    int lim;
    int d;
    int r;
    lim = ln[p] + ln[q];
    for (d = 0; d < lim; ++d)
    {
        r = diff_at(s, ws, ln, p, q, d);
        if (r != 0)
        {
            return r;
        }
    }
    return 0;
}

/* --- Duval Lyndon factorization ---------------------------------------- */

/* One comparison step of Duval's scan for the word starting at p. Advances
 * the window pointers *a, *b; returns 0 when the window must close. */
static int duval_step(const unsigned char *s, int n, int p, int *a, int *b)
{
    int x;
    int y;
    if (*b >= n)
    {
        return 0;
    }
    x = s[*a];
    y = s[*b];
    if (x > y)
    {
        return 0;
    }
    if (x < y)
    {
        *a = p;
    }
    else
    {
        ++*a;
    }
    ++*b;
    return 1;
}

/* Record one word start at position p and return the next position. */
static int record_start(int *starts, int *nw, int p, int len)
{
    starts[*nw] = p;
    ++*nw;
    return p + len;
}

/* Emit the starts of the Lyndon word(s) beginning at *pp and advance *pp. */
static void factor_word(const unsigned char *s, int n, int *pp, int *starts,
                        int *nw)
{
    int p;
    int a;
    int b;
    int len;
    int cont;
    p = *pp;
    a = p;
    b = p + 1;
    cont = 1;
    while (cont != 0)
    {
        cont = duval_step(s, n, p, &a, &b);
    }
    len = b - a;
    while (p <= a)
    {
        p = record_start(starts, nw, p, len);
    }
    *pp = p;
}

/* Fill ws (word-start index) and ln (word length) for every position. */
/* Fill ws (word-start index) and ln (word length) for every position. */
static void fill_word(int *ws, int *ln, const int *starts, int i, int *w)
{
    while (starts[*w + 1] <= i)
    {
        ++*w;
    }
    ws[i] = starts[*w];
    ln[i] = starts[*w + 1] - starts[*w];
}

static void factorize(const unsigned char *s, int n, int *ws, int *ln,
                      int *starts)
{
    int nw;
    int p;
    int w;
    int i;
    nw = 0;
    p = 0;
    while (p < n)
    {
        factor_word(s, n, &p, starts, &nw);
    }
    starts[nw] = n;
    w = 0;
    for (i = 0; i < n; ++i)
    {
        fill_word(ws, ln, starts, i, &w);
    }
}

/* --- conjugate ordering (merge sort) ----------------------------------- */

/* Merge one element of the two sorted runs [i,mid) and [mid,hi) into tmp,
 * advancing the chosen source. */
static int take_right(const unsigned char *s, const int *ws, const int *ln,
                      const int *ord, int i, int j, int mid, int hi)
{
    int cmp;
    if (j >= hi)
    {
        return 0;
    }
    if (i >= mid)
    {
        return 1;
    }
    cmp = conj_cmp(s, ws, ln, ord[j], ord[i]);
    if (cmp < 0)
    {
        return 1;
    }
    return 0;
}

static void copy_to_tmp(int *tmp, const int *ord, int dst, int src)
{
    tmp[dst] = ord[src];
}

static void copy_from_tmp(int *ord, const int *tmp, int t)
{
    ord[t] = tmp[t];
}

static void merge_one(const unsigned char *s, const int *ws, const int *ln,
                      const int *ord, int *tmp, int mid, int hi, int *pi,
                      int *pj, int *pk)
{
    int i;
    int j;
    int k;
    int from_right;
    int src;
    i = *pi;
    j = *pj;
    k = *pk;
    from_right = take_right(s, ws, ln, ord, i, j, mid, hi);
    if (from_right != 0)
    {
        src = j;
    }
    else
    {
        src = i;
    }
    copy_to_tmp(tmp, ord, k, src);
    if (from_right != 0)
    {
        ++j;
    }
    else
    {
        ++i;
    }
    ++k;
    *pi = i;
    *pj = j;
    *pk = k;
}

/* Merge the sorted runs [lo,mid) and [mid,hi) into [lo,hi) via tmp. */
static void merge_run(const unsigned char *s, const int *ws, const int *ln,
                      int *ord, int *tmp, int lo, int mid, int hi)
{
    int i;
    int j;
    int k;
    int t;
    for (t = lo; t < hi; ++t)
    {
        copy_to_tmp(tmp, ord, t, t);
    }
    i = lo;
    j = mid;
    k = lo;
    while (k < hi)
    {
        merge_one(s, ws, ln, ord, tmp, mid, hi, &i, &j, &k);
    }
    for (t = lo; t < hi; ++t)
    {
        copy_from_tmp(ord, tmp, t);
    }
}

/* Recursive top-down merge sort of ord[lo..hi) by conj_cmp. */
static void sort_run(const unsigned char *s, const int *ws, const int *ln,
                     int *ord, int *tmp, int lo, int hi)
{
    int mid;
    if (hi - lo <= 1)
    {
        return;
    }
    mid = lo + (hi - lo) / 2;
    sort_run(s, ws, ln, ord, tmp, lo, mid);
    sort_run(s, ws, ln, ord, tmp, mid, hi);
    merge_run(s, ws, ln, ord, tmp, lo, mid, hi);
}

static void sort_positions(const unsigned char *s, const int *ws, const int *ln,
                           int *ord, int *tmp, int n)
{
    sort_run(s, ws, ln, ord, tmp, 0, n);
}

static void emit_conjugate(unsigned char *out, const unsigned char *s,
                           const int *ws, const int *ln, const int *ord, int i)
{
    int p;
    int last;
    p = ord[i];
    last = ln[p] - 1;
    out[i] = conj_byte(s, ws, ln, p, last);
}

/* --- inverse (LF cycles) ----------------------------------------------- */

static int prefix_next(int *less, const int *freq, int c, int acc)
{
    less[c] = acc;
    return acc + freq[c];
}

static void rank_assign(int *rank, int *less, const unsigned char *b, int i)
{
    rank[i] = less[b[i]];
    ++less[b[i]];
}

/* Write one byte of a cycle; advance pos or finish. */
static void walk_one(const unsigned char *b, unsigned char *out, int *rank,
                     int *pi, int *wi, int *done)
{
    int pos;
    int k;
    int nxt;
    pos = *pi;
    k = *wi - 1;
    out[k] = b[pos];
    nxt = rank[pos];
    rank[pos] = -1;
    *wi = k;
    if (nxt < 0)
    {
        *done = 1;
        return;
    }
    if (rank[nxt] < 0)
    {
        *done = 1;
        return;
    }
    *pi = nxt;
}

static void run_cycle(const unsigned char *b, unsigned char *out, int *rank,
                      int *wi, int start)
{
    int pos;
    int done;
    pos = start;
    done = 0;
    while (done == 0)
    {
        walk_one(b, out, rank, &pos, wi, &done);
    }
}

static void run_cycles(const unsigned char *b, unsigned char *out, int *rank,
                       int n)
{
    int wi;
    int i;
    wi = n;
    for (i = 0; i < n; ++i)
    {
        if (rank[i] >= 0)
        {
            run_cycle(b, out, rank, &wi, i);
        }
    }
}

/* --- public entry points ----------------------------------------------- */

/* Make a private copy of the input so emission/cycle reads never observe
 * freshly written output when the caller passes out == in. Called at top
 * level (root scope) of the entry points. */
static unsigned char *copy_input(const unsigned char *s, size_t n)
{
    unsigned char *copy;
    copy = bwt89_malloc(n);
    if (copy != NULL)
    {
        memcpy(copy, s, n);
    }
    return copy;
}

enum bwt89_status bwt89_bbwt(const unsigned char *s, size_t n,
                             unsigned char *out)
{
    int *block;
    int *ws;
    int *ln;
    int *ord;
    int *tmp;
    int *starts;
    unsigned char *src;
    int N;
    size_t total;
    int i;
    if (s == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (out == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (n > BWT89_MAX_N)
    {
        return BWT89_TOO_LARGE;
    }
    if (n == 0)
    {
        return BWT89_OK;
    }
    N = (int)n;
    total = (size_t)5 * n + 1;
    block = (int *)bwt89_malloc(total * sizeof(int));
    if (block == NULL)
    {
        return BWT89_NOMEM;
    }
    ws = block;
    ln = block + N;
    ord = block + 2 * N;
    tmp = block + 3 * N;
    starts = block + 4 * N;
    src = copy_input(s, (size_t)N);
    if (src == NULL)
    {
        bwt89_free(block);
        return BWT89_NOMEM;
    }
    factorize(src, N, ws, ln, starts);
    for (i = 0; i < N; ++i)
    {
        ord[i] = i;
    }
    sort_positions(src, ws, ln, ord, tmp, N);
    for (i = 0; i < N; ++i)
    {
        emit_conjugate(out, src, ws, ln, ord, i);
    }
    bwt89_free(src);
    bwt89_free(block);
    return BWT89_OK;
}

enum bwt89_status bwt89_ibbwt(const unsigned char *b, size_t n,
                              unsigned char *out)
{
    int *rank;
    int *block;
    int freq[BWT89_ALPHABET];
    int less[BWT89_ALPHABET];
    unsigned char *src;
    int N;
    int c;
    int i;
    int acc;
    if (b == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (out == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (n > BWT89_MAX_N)
    {
        return BWT89_TOO_LARGE;
    }
    if (n == 0)
    {
        return BWT89_OK;
    }
    N = (int)n;
    block = (int *)bwt89_malloc((size_t)N * sizeof(int));
    if (block == NULL)
    {
        return BWT89_NOMEM;
    }
    rank = block;
    src = copy_input(b, (size_t)N);
    if (src == NULL)
    {
        bwt89_free(block);
        return BWT89_NOMEM;
    }
    for (c = 0; c < BWT89_ALPHABET; ++c)
    {
        freq[c] = 0;
    }
    for (i = 0; i < N; ++i)
    {
        ++freq[src[i]];
    }
    acc = 0;
    for (c = 0; c < BWT89_ALPHABET; ++c)
    {
        acc = prefix_next(less, freq, c, acc);
    }
    for (i = 0; i < N; ++i)
    {
        rank_assign(rank, less, src, i);
    }
    run_cycles(src, out, rank, N);
    bwt89_free(src);
    bwt89_free(block);
    return BWT89_OK;
}
