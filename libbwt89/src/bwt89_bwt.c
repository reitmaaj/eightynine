/* bwt89_bwt.c - regular Burrows-Wheeler transform and its inverse.
 *
 * An internal sentinel (value 0) is appended; real bytes map to 1..256. The
 * suffix array of the (n+1)-symbol string is computed with SA-IS. The output
 * is the last column of the sorted-rotation matrix with the sentinel's
 * contribution removed: n bytes, a permutation of the input. The original
 * row is returned as `index`; the inverse re-inserts the sentinel there and
 * recovers the text by LF-mapping.
 *
 * Written to the green worker/controller discipline: workers compute at
 * their own top level; decision/iteration bodies delegate in single glue
 * statements. */

#include "bwt89_internal.h"
#include <bwt89.h>

enum
{
    BWT89_SENTINEL = 0,
    BWT89_ALPHA = 256
};

/* --- pure value workers ------------------------------------------------ */

GREEN_PURE
static int rank_byte(unsigned char b)
{
    return (int)b + 1;
}

GREEN_PURE
static size_t real_slot(size_t k, size_t index)
{
    if (k < index)
    {
        return k;
    }
    return k + 1;
}

GREEN_PURE
static size_t output_slot(size_t row, size_t orig)
{
    if (row < orig)
    {
        return row;
    }
    return row - 1;
}

GREEN_PURE
static int addv(int a, int b)
{
    return a + b;
}

GREEN_PURE
static int lower_count(const int *freq, int c)
{
    int acc;
    int k;
    acc = 0;
    for (k = 0; k < c; ++k)
    {
        acc = addv(acc, freq[k]);
    }
    return acc;
}

/* --- forward transform -------------------------------------------------- */

static void build_symbols(const unsigned char *s, size_t n, int *sym)
{
    size_t i;
    for (i = 0; i < n; ++i)
    {
        sym[i] = rank_byte(s[i]);
    }
    sym[n] = BWT89_SENTINEL;
}

GREEN_PURE
static size_t locate_orig(const int *sa, int N)
{
    size_t i;
    for (i = 0; i < (size_t)N; ++i)
    {
        if (sa[i] == 0)
        {
            break;
        }
    }
    return i;
}

static void emit_row(unsigned char *out, const int *sym, int p, int N,
                     size_t row, size_t orig)
{
    int c;
    size_t slot;
    if (row == orig)
    {
        return;
    }
    slot = output_slot(row, orig);
    c = sym[(p + N - 1) % N];
    out[slot] = (unsigned char)(c - 1);
}

enum bwt89_status bwt89_bwt(const unsigned char *s, size_t n, size_t *index,
                            unsigned char *out)
{
    int *sym;
    int *sa;
    int *block;
    int N;
    size_t i;
    size_t orig;
    int r;
    if (s == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (out == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (index == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (n > BWT89_MAX_N)
    {
        return BWT89_TOO_LARGE;
    }
    if (n == 0)
    {
        *index = 0;
    }
    if (n == 0)
    {
        return BWT89_OK;
    }
    bwt89_note_bwt();
    N = (int)n + 1;
    block = (int *)bwt89_malloc(2 * (size_t)N * sizeof(int));
    if (block == NULL)
    {
        return BWT89_NOMEM;
    }
    sym = block;
    sa = block + N;
    build_symbols(s, n, sym);
    r = bwt89_sa_is(sym, sa, N, BWT89_ALPHA);
    if (r != 0)
    {
        bwt89_free(block);
        return BWT89_NOMEM;
    }
    orig = locate_orig(sa, N);
    *index = orig;
    for (i = 0; i < (size_t)N; ++i)
    {
        emit_row(out, sym, sa[i], N, i, orig);
    }
    bwt89_free(block);
    return BWT89_OK;
}

/* --- inverse transform -------------------------------------------------- */

static void build_last(int *last, const unsigned char *b, size_t n,
                       size_t index)
{
    size_t k;
    last[index] = BWT89_SENTINEL;
    for (k = 0; k < n; ++k)
    {
        last[real_slot(k, index)] = rank_byte(b[k]);
    }
}

static void tally_last(const int *last, int N, int *freq)
{
    int i;
    for (i = 0; i < N; ++i)
    {
        ++freq[last[i]];
    }
}

static void build_less(const int *freq, int *less, int M)
{
    int c;
    for (c = 0; c <= M; ++c)
    {
        less[c] = lower_count(freq, c);
    }
}

static void zero_occ(int *occ, int M)
{
    int c;
    for (c = 0; c <= M; ++c)
    {
        occ[c] = 0;
    }
}

static void map_row(int *occ, const int *less, int *row_of, int sym, int i)
{
    int r;
    r = less[sym] + occ[sym];
    row_of[i] = r;
    ++occ[sym];
}

static int walk_row(const int *row_of, const int *last, int row, size_t pos,
                    unsigned char *out)
{
    int c;
    row = row_of[row];
    c = last[row];
    if (c == BWT89_SENTINEL)
    {
        return -1;
    }
    out[pos] = (unsigned char)(c - 1);
    return row;
}

/* Advance one LF step without writing output; -1 marks the sentinel. Used to
 * validate a (b, index) pair before any output is written. */
static int step_peek(const int *row_of, const int *last, int row)
{
    int c;
    row = row_of[row];
    c = last[row];
    if (c == BWT89_SENTINEL)
    {
        return -1;
    }
    return row;
}

enum bwt89_status bwt89_ibwt(const unsigned char *b, size_t n, size_t index,
                             unsigned char *out)
{
    int *block;
    int *last;
    int *row_of;
    int *freq;
    int *less;
    int *occ;
    int N;
    int M;
    size_t i;
    int row;
    if (b == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (out == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (n == 0)
    {
        if (index != 0)
        {
            return BWT89_BAD_INDEX;
        }
        return BWT89_OK;
    }
    if (index > n)
    {
        return BWT89_BAD_INDEX;
    }
    if (n > BWT89_MAX_N)
    {
        return BWT89_TOO_LARGE;
    }
    N = (int)n + 1;
    M = BWT89_ALPHA;
    block = (int *)bwt89_malloc((2 * (size_t)N + 3 * (size_t)(M + 1)) *
                                sizeof(int));
    if (block == NULL)
    {
        return BWT89_NOMEM;
    }
    last = block;
    row_of = block + N;
    freq = block + 2 * N;
    less = freq + (M + 1);
    occ = less + (M + 1);
    build_last(last, b, n, index);
    zero_occ(freq, M);
    tally_last(last, N, freq);
    build_less(freq, less, M);
    zero_occ(occ, M);
    for (i = 0; i < (size_t)N; ++i)
    {
        map_row(occ, less, row_of, last[i], (int)i);
    }
    row = (int)index;
    for (i = 0; i < n; ++i)
    {
        row = step_peek(row_of, last, row);
        if (row < 0)
        {
            break;
        }
    }
    if (row < 0)
    {
        bwt89_free(block);
        return BWT89_BAD_DATA;
    }
    row = (int)index;
    for (i = 0; i < n; ++i)
    {
        row = walk_row(row_of, last, row, n - 1 - i, out);
        if (row < 0)
        {
            break;
        }
    }
    bwt89_free(block);
    return BWT89_OK;
}
