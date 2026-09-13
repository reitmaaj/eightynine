/* bwt89_sa.c - linear-time suffix array construction (SA-IS,
 * Nong-Zhang-Chan 2009). Contract: s[0..n-1], n >= 1, s[n-1] == 0 is the
 * unique minimum symbol and all symbols lie in [0, K]. Computes the suffix
 * array into sa[0..n-1] (sa[0] == n-1, the sentinel). O(n) time, O(n) space.
 *
 * Written to the green worker/controller discipline: workers compute at their
 * own top level; decision/iteration bodies delegate in single glue
 * statements. Algorithm structure matches a standard SA-IS decomposition. */

#include <stdlib.h>

#include "bwt89_internal.h"

enum bwt89_sa_type
{
    BWT89_SA_L = 0,
    BWT89_SA_S = 1
};

/* --- forward declarations (all static workers) ------------------------- */

static void classify_types(const int *s, int n, int *t);
static void zero_range(int *a, int len);
static void fill_neg(int *a, int len);
static void count_symbols(const int *s, int n, int *cnt);
static void make_heads(const int *cnt, int K, int *bkt);
static void make_tails(const int *cnt, int K, int *bkt);
static void place_head(int *bkt, int *sa, int sym, int val);
static void place_tail(int *bkt, int *sa, int sym, int val);
static void record_lms(int *ls, int i, int *k);
static void place_lms(int *ls, const int *t, int n, int i, int *k);
static void append_sorted(int *ls, const int *sa, const int *t, int n, int i,
                          int *k);
static void seed_one(int *sa, int *bkt, const int *s, const int *t, int n,
                     int i);
static void induced_L_step(int *sa, int *bkt, const int *s, const int *t,
                           int i);
static void induced_S_step(int *sa, int *bkt, const int *s, const int *t,
                           int i);
static void name_one(int *sa, const int *ls, const int *s, const int *t, int n,
                     int i, int *num);
static void build_s1(int *s1, const int *sa, const int *t, int n, int i,
                     int *k);
static void build_inverse(const int *s1, int *sa1, int m);
static void final_seed_one(int *sa, int *bkt, const int *s, const int *ls,
                           const int *sa1, int rank);
static int sa_is_core(const int *s, int *sa, int n, int K);

/* --- pure value workers ------------------------------------------------ */

GREEN_PURE
static int addv(int a, int b)
{
    return a + b;
}

GREEN_PURE
static int classify_one(int a, int b, int nb)
{
    if (a < b)
    {
        return BWT89_SA_S;
    }
    if (a > b)
    {
        return BWT89_SA_L;
    }
    return nb;
}

GREEN_PURE
static int lms_start(const int *t, int n, int x)
{
    if (t[x] != BWT89_SA_S)
    {
        return 0;
    }
    if (x == n - 1)
    {
        return 1;
    }
    if (x == 0)
    {
        return 0;
    }
    if (t[x - 1] == BWT89_SA_L)
    {
        return 1;
    }
    return 0;
}

GREEN_PURE
static int find_next_lms(const int *t, int n, int start)
{
    int i;
    for (i = start + 1; i < n; ++i)
    {
        if (lms_start(t, n, i) != 0)
        {
            return i;
        }
    }
    return n;
}

GREEN_PURE
static int lms_equal(const int *s, const int *t, int n, int a, int b)
{
    int ea;
    int eb;
    int i;
    ea = find_next_lms(t, n, a);
    eb = find_next_lms(t, n, b);
    if (ea - a != eb - b)
    {
        return 0;
    }
    for (i = a; i < ea; ++i)
    {
        if (s[i] != s[b + (i - a)])
        {
            return 0;
        }
    }
    return 1;
}

GREEN_PURE
static int next_name(const int *s, const int *t, int n, int a, int b, int cur)
{
    if (lms_equal(s, t, n, a, b) != 0)
    {
        return cur;
    }
    return cur + 1;
}

/* --- mutating workers -------------------------------------------------- */

static void classify_types(const int *s, int n, int *t)
{
    int i;
    t[n - 1] = BWT89_SA_S;
    for (i = n - 2; i >= 0; --i)
    {
        t[i] = classify_one(s[i], s[i + 1], t[i + 1]);
    }
}

static void zero_range(int *a, int len)
{
    int i;
    for (i = 0; i < len; ++i)
    {
        a[i] = 0;
    }
}

static void fill_neg(int *a, int len)
{
    int i;
    for (i = 0; i < len; ++i)
    {
        a[i] = -1;
    }
}

static void count_symbols(const int *s, int n, int *cnt)
{
    int i;
    for (i = 0; i < n; ++i)
    {
        ++cnt[s[i]];
    }
}

static void make_heads(const int *cnt, int K, int *bkt)
{
    int c;
    bkt[0] = 0;
    for (c = 1; c <= K; ++c)
    {
        bkt[c] = addv(bkt[c - 1], cnt[c - 1]);
    }
}

static void make_tails(const int *cnt, int K, int *bkt)
{
    int c;
    bkt[0] = cnt[0];
    for (c = 1; c <= K; ++c)
    {
        bkt[c] = addv(bkt[c - 1], cnt[c]);
    }
}

static void place_head(int *bkt, int *sa, int sym, int val)
{
    int j;
    j = bkt[sym];
    sa[j] = val;
    ++bkt[sym];
}

static void place_tail(int *bkt, int *sa, int sym, int val)
{
    int j;
    --bkt[sym];
    j = bkt[sym];
    sa[j] = val;
}

static void record_lms(int *ls, int i, int *k)
{
    ls[*k] = i;
    ++*k;
}

static void place_lms(int *ls, const int *t, int n, int i, int *k)
{
    if (lms_start(t, n, i) != 0)
    {
        record_lms(ls, i, k);
    }
}

static void append_sorted(int *ls, const int *sa, const int *t, int n, int i,
                          int *k)
{
    int j;
    j = sa[i];
    if (j >= 0)
    {
        place_lms(ls, t, n, j, k);
    }
}

static void seed_one(int *sa, int *bkt, const int *s, const int *t, int n,
                     int i)
{
    if (lms_start(t, n, i) != 0)
    {
        place_tail(bkt, sa, s[i], i);
    }
}

static void induced_L_step(int *sa, int *bkt, const int *s, const int *t, int i)
{
    int p;
    int prev;
    p = sa[i];
    if (p <= 0)
    {
        return;
    }
    prev = p - 1;
    if (t[prev] != BWT89_SA_L)
    {
        return;
    }
    place_head(bkt, sa, s[prev], prev);
}

static void induced_S_step(int *sa, int *bkt, const int *s, const int *t, int i)
{
    int p;
    int prev;
    p = sa[i];
    if (p <= 0)
    {
        return;
    }
    prev = p - 1;
    if (t[prev] != BWT89_SA_S)
    {
        return;
    }
    place_tail(bkt, sa, s[prev], prev);
}

static void name_one(int *sa, const int *ls, const int *s, const int *t, int n,
                     int i, int *num)
{
    int a;
    int b;
    int cur;
    int nxt;
    a = ls[i - 1];
    b = ls[i];
    cur = *num;
    nxt = next_name(s, t, n, a, b, cur);
    sa[b] = nxt;
    *num = nxt;
}

static void build_s1(int *s1, const int *sa, const int *t, int n, int i, int *k)
{
    if (lms_start(t, n, i) != 0)
    {
        record_lms(s1, sa[i], k);
    }
}

static void build_inverse(const int *s1, int *sa1, int m)
{
    int r;
    for (r = 0; r < m; ++r)
    {
        sa1[s1[r]] = r;
    }
}

static void final_seed_one(int *sa, int *bkt, const int *s, const int *ls,
                           const int *sa1, int rank)
{
    int i;
    i = ls[sa1[rank]];
    place_tail(bkt, sa, s[i], i);
}

/* --- recursive core ---------------------------------------------------- */

static int sa_is_core(const int *s, int *sa, int n, int K)
{
    int *arena;
    int *t;
    int *ls;
    int *s1;
    int *sa1;
    int *bkt;
    int *cnt;
    int m;
    int i;
    int k;
    int num;
    int rank;
    int err;
    int distinct;
    size_t total;
    if (n == 1)
    {
        sa[0] = 0;
    }
    if (n <= 1)
    {
        return 0;
    }
    total = (size_t)4 * (size_t)n + (size_t)2 * (size_t)(K + 1);
    arena = (int *)bwt89_malloc(total * sizeof(int));
    if (arena == NULL)
    {
        return 1;
    }
    t = arena;
    ls = arena + n;
    s1 = arena + 2 * n;
    sa1 = arena + 3 * n;
    bkt = arena + 4 * n;
    cnt = bkt + (K + 1);
    classify_types(s, n, t);
    zero_range(cnt, K + 1);
    count_symbols(s, n, cnt);
    k = 0;
    for (i = 0; i < n; ++i)
    {
        place_lms(ls, t, n, i, &k);
    }
    m = k;
    fill_neg(sa, n);
    make_tails(cnt, K, bkt);
    for (i = n - 1; i >= 0; --i)
    {
        seed_one(sa, bkt, s, t, n, i);
    }
    make_heads(cnt, K, bkt);
    for (i = 0; i < n; ++i)
    {
        induced_L_step(sa, bkt, s, t, i);
    }
    make_tails(cnt, K, bkt);
    for (i = n - 1; i >= 0; --i)
    {
        induced_S_step(sa, bkt, s, t, i);
    }
    k = 0;
    for (i = 0; i < n; ++i)
    {
        append_sorted(ls, sa, t, n, i, &k);
    }
    m = k;
    fill_neg(sa, n);
    num = 0;
    if (m > 0)
    {
        sa[ls[0]] = 0;
    }
    for (i = 1; i < m; ++i)
    {
        name_one(sa, ls, s, t, n, i, &num);
    }
    k = 0;
    for (i = 0; i < n; ++i)
    {
        build_s1(s1, sa, t, n, i, &k);
    }
    m = k;
    distinct = 0;
    if (num + 1 == m)
    {
        distinct = 1;
    }
    if (distinct != 0)
    {
        build_inverse(s1, sa1, m);
    }
    err = 0;
    if (distinct == 0)
    {
        err = sa_is_core(s1, sa1, m, num);
    }
    if (err != 0)
    {
        bwt89_free(arena);
        return err;
    }
    k = 0;
    for (i = 0; i < n; ++i)
    {
        place_lms(ls, t, n, i, &k);
    }
    m = k;
    fill_neg(sa, n);
    make_tails(cnt, K, bkt);
    for (rank = m - 1; rank >= 0; --rank)
    {
        final_seed_one(sa, bkt, s, ls, sa1, rank);
    }
    make_heads(cnt, K, bkt);
    for (i = 0; i < n; ++i)
    {
        induced_L_step(sa, bkt, s, t, i);
    }
    make_tails(cnt, K, bkt);
    for (i = n - 1; i >= 0; --i)
    {
        induced_S_step(sa, bkt, s, t, i);
    }
    bwt89_free(arena);
    return 0;
}

int bwt89_sa_is(const int *s, int *sa, int n, int K)
{
    int r;
    bwt89_note_sais();
    r = sa_is_core(s, sa, n, K);
    return r;
}
