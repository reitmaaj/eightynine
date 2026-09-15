/* u89_nfc.c - NFC/NFD/NFKC/NFKD normalization (UAX #15).

   Written in green worker/controller shape: straight-line computation lives at
   a function top level; every nested (loop/if) body reduces to a single
   delegation, binding, or terminal return. Normalization is gated by the full
   NormalizationTest.txt conformance suite (just conform-norm). */

#include "../include/u89.h"
#include "u89_tables.h"

#define SBASE 0xAC00U
#define LBASE 0x1100U
#define VBASE 0x1161U
#define TBASE 0x11A7U
#define LCOUNT 19U
#define VCOUNT 21U
#define TCOUNT 28U
#define NCOUNT (VCOUNT * TCOUNT)
#define SCOUNT (LCOUNT * NCOUNT)

static int in_range(u89_cp v, u89_cp lo, u89_cp hi)
{
    if (v < lo)
    {
        return 0;
    }
    if (v >= hi)
    {
        return 0;
    }
    return 1;
}

static int hangul_syllable(u89_cp cp)
{
    if (cp < SBASE)
    {
        return 0;
    }
    if (cp >= SBASE + SCOUNT)
    {
        return 0;
    }
    return 1;
}

/* ---- Canonical combining class ------------------------------------------- */

/* Probe the sorted nonzero-ccc ranges. Returns -1 when cp precedes range i
   (all later ranges exceed cp: ccc 0), 0 to continue, or the nonzero ccc when
   cp lies in range i. */
static int ccc_probe(const u89_crange *t, size_t i, u89_cp cp)
{
    if (cp < t[i].lo)
    {
        return -1;
    }
    if (cp > t[i].hi)
    {
        return 0;
    }
    return t[i].ccc;
}

static int get_ccc(u89_cp cp)
{
    size_t i;
    int code;

    for (i = 0; i < u89_ccc_count; ++i)
    {
        code = ccc_probe(u89_ccc_ranges, i, cp);
        if (code < 0)
        {
            return 0;
        }
        if (code != 0)
        {
            return code;
        }
    }
    return 0;
}

/* ---- Decomposition ------------------------------------------------------- */

/* Probe the sorted decomposition table by code point. Returns 1 found, 2 stop
   (key precedes), 0 continue. */
static int decomp_at(const u89_mapping *t, size_t i, u89_cp cp)
{
    if (cp < t[i].cp)
    {
        return 2;
    }
    if (cp > t[i].cp)
    {
        return 0;
    }
    return 1;
}

/* Copy scratch[k] = src[k] and return the next index. */
static size_t decomp_copy(const u89_cp *src, u89_cp *scratch, size_t k)
{
    scratch[k] = src[k];
    return k + 1;
}

/* Append tmp[0..cnt) into w at offset m (or just count when w is null).
   Returns the new length. */
static size_t append_scalars(u89_cp *w, size_t m, const u89_cp *tmp, size_t cnt)
{
    size_t k;

    if (w == NULL)
    {
        return m + cnt;
    }
    k = 0;
    while (k < cnt)
    {
        k = decomp_copy(tmp, w + m, k);
    }
    return m + cnt;
}

/* Emit the stored decomposition of table entry i into scratch. Returns the
   scalar count, or -1 if scratch is too small. */
static int decomp_emit(const u89_mapping *t, size_t i, const u89_cp *pool,
                       u89_cp *scratch, size_t slots)
{
    size_t len;
    size_t k;
    unsigned long off;

    len = t[i].length;
    if (slots < len)
    {
        return -1;
    }
    off = t[i].offset;
    k = 0;
    while (k < len)
    {
        k = decomp_copy(pool + off, scratch, k);
    }
    return (int)len;
}

/* Decompose one Hangul syllable into scratch. Returns the count (2 or 3),
   -1 if scratch is too small, or 0 when cp is not a Hangul syllable. */
static int hangul_decomp(u89_cp cp, u89_cp *scratch, size_t slots)
{
    u89_cp sindex;
    u89_cp l;
    u89_cp v;
    u89_cp t2;
    int hs;
    int n;

    hs = hangul_syllable(cp);
    if (hs == 0)
    {
        return 0;
    }
    sindex = cp - SBASE;
    l = LBASE + sindex / NCOUNT;
    v = VBASE + (sindex % NCOUNT) / TCOUNT;
    t2 = TBASE + sindex % TCOUNT;
    n = 2;
    if (t2 != TBASE)
    {
        n = 3;
    }
    if ((size_t)n > slots)
    {
        return -1;
    }
    scratch[0] = l;
    scratch[1] = v;
    if (n == 3)
    {
        scratch[2] = t2;
    }
    return n;
}

/* Advance the decomposition scan. Returns 0 once *i is settled (found, or past
   the last possible row meaning identity), else 1 having moved *i onward. */
static int decomp_scan(const u89_mapping *t, size_t n, u89_cp cp, size_t *i,
                       int *found)
{
    int eq;

    if (*i >= n)
    {
        *found = 0;
        return 0;
    }
    eq = decomp_at(t, *i, cp);
    if (eq == 2)
    {
        *found = 0;
        return 0;
    }
    if (eq == 1)
    {
        *found = 1;
        return 0;
    }
    *i = *i + 1;
    return 1;
}

/* Return the index of cp in the sorted decomposition table, or n when cp is
   absent (identity decomposition). */
static size_t decomp_find_index(const u89_mapping *t, size_t n, u89_cp cp)
{
    size_t i;
    int found;
    int go;

    i = 0;
    found = 0;
    go = 1;
    while (go != 0)
    {
        go = decomp_scan(t, n, cp, &i, &found);
    }
    if (found)
    {
        return i;
    }
    return n;
}

/* Identity decomposition: a scalar not present in a decomposition table maps
   to itself. Returns 1, or -1 when scratch has no room. */
static int identity_decomp(u89_cp cp, u89_cp *scratch, size_t slots)
{
    if (slots < 1)
    {
        return -1;
    }
    scratch[0] = cp;
    return 1;
}

/* Fully decompose one scalar into scratch; returns the count written, -1 when
   scratch (slots) is too small. Uses the compatibility table when compat. */
static int decompose_one(u89_cp cp, int compat, u89_cp *scratch, size_t slots)
{
    const u89_mapping *t;
    size_t n;
    size_t i;
    int h;
    int hid;
    int got;

    h = hangul_decomp(cp, scratch, slots);
    if (h != 0)
    {
        return h;
    }
    if (compat)
    {
        t = u89_compat;
    }
    else
    {
        t = u89_canon;
    }
    if (compat)
    {
        n = u89_compat_count;
    }
    else
    {
        n = u89_canon_count;
    }
    i = decomp_find_index(t, n, cp);
    if (i == n)
    {
        hid = identity_decomp(cp, scratch, slots);
        return hid;
    }
    got = decomp_emit(t, i, u89_mapping_pool, scratch, slots);
    if (got < 0)
    {
        return -1;
    }
    return got;
}

/* ---- Canonical reordering ------------------------------------------------ */

/* Advance *i while w[*i] is a non-starter; returns 0 at a starter or end. */
static int run_next(const u89_cp *w, size_t count, size_t *i)
{
    int c;

    if (*i >= count)
    {
        return 0;
    }
    c = get_ccc(w[*i]);
    if (c == 0)
    {
        return 0;
    }
    *i = *i + 1;
    return 1;
}

/* Return one past the maximal non-starter run beginning at i. */
static size_t run_end(const u89_cp *w, size_t count, size_t i)
{
    int go;

    go = 1;
    while (go != 0)
    {
        go = run_next(w, count, &i);
    }
    return i;
}

/* Bubble w[b] left one place while its ccc is smaller than the previous.
   Returns b - 1 when a swap happened, else lo (nothing to move). */
static size_t sort_step(u89_cp *w, size_t b, size_t lo)
{
    int cb;
    int cb1;
    u89_cp tmp;

    if (b <= lo)
    {
        return lo;
    }
    cb = get_ccc(w[b]);
    cb1 = get_ccc(w[b - 1]);
    if (cb >= cb1)
    {
        return lo;
    }
    tmp = w[b];
    w[b] = w[b - 1];
    w[b - 1] = tmp;
    return b - 1;
}

/* Insert the element at a into the already-sorted prefix [lo, a). Returns the
   next insertion position a + 1. */
static size_t sort_insert(u89_cp *w, size_t a, size_t lo)
{
    size_t b;

    b = a;
    while (b > lo)
    {
        b = sort_step(w, b, lo);
    }
    return a + 1;
}

/* Stable insertion sort of the non-starter run w[lo..hi) by ccc. */
static void sort_run(u89_cp *w, size_t lo, size_t hi)
{
    size_t a;

    a = lo + 1;
    while (a < hi)
    {
        a = sort_insert(w, a, lo);
    }
}

/* Advance the reorder scan past w[i]: a starter advances alone, a non-starter
   run is sorted. Returns the next scan index. */
static size_t reorder_advance(u89_cp *w, size_t count, size_t i)
{
    int c;
    size_t hi;

    c = get_ccc(w[i]);
    if (c == 0)
    {
        return i + 1;
    }
    hi = run_end(w, count, i);
    sort_run(w, i, hi);
    return hi;
}

static void reorder(u89_cp *w, size_t count)
{
    size_t i;

    i = 0;
    while (i < count)
    {
        i = reorder_advance(w, count, i);
    }
}

/* ---- Canonical composition ----------------------------------------------- */

static u89_cp lv_pair(u89_cp first, u89_cp second)
{
    int a;
    int b;

    a = in_range(first, LBASE, LBASE + LCOUNT);
    if (a == 0)
    {
        return 0;
    }
    b = in_range(second, VBASE, VBASE + VCOUNT);
    if (b == 0)
    {
        return 0;
    }
    return SBASE + ((first - LBASE) * VCOUNT + (second - VBASE)) * TCOUNT;
}

static u89_cp lvt_pair(u89_cp first, u89_cp second)
{
    u89_cp sindex;
    int hs;

    hs = hangul_syllable(first);
    if (hs == 0)
    {
        return 0;
    }
    sindex = first - SBASE;
    if (sindex % TCOUNT != 0)
    {
        return 0;
    }
    if (second <= TBASE)
    {
        return 0;
    }
    if (second >= TBASE + TCOUNT)
    {
        return 0;
    }
    return first + (second - TBASE);
}

/* Return the Hangul composition of first+second, or 0 when it does not apply.
 */
static u89_cp hangul_pair(u89_cp first, u89_cp second)
{
    u89_cp r;
    u89_cp r2;

    r = lv_pair(first, second);
    if (r != 0)
    {
        return r;
    }
    r2 = lvt_pair(first, second);
    return r2;
}

/* Probe the sorted composition table (by first, then second). Returns 0 to
   continue, 1 found, 2 stop (no further row can match). */
static int comp_probe(const u89_comp *t, size_t i, u89_cp first, u89_cp second)
{
    if (t[i].first < first)
    {
        return 0;
    }
    if (t[i].first > first)
    {
        return 2;
    }
    if (t[i].second < second)
    {
        return 0;
    }
    if (t[i].second > second)
    {
        return 2;
    }
    return 1;
}

/* Write the stored composite for row i into *result. */
static void comp_result(const u89_comp *t, size_t i, u89_cp *result)
{
    u89_cp r;

    r = t[i].result;
    *result = r;
}

static int compose_pair(u89_cp first, u89_cp second, u89_cp *result)
{
    size_t i;
    int code;
    u89_cp hr;

    hr = hangul_pair(first, second);
    if (hr != 0)
    {
        *result = hr;
        return 1;
    }
    for (i = 0; i < u89_comp_count; ++i)
    {
        code = comp_probe(u89_comp_tbl, i, first, second);
        if (code == 1)
        {
            comp_result(u89_comp_tbl, i, result);
            return 1;
        }
        if (code == 2)
        {
            return 0;
        }
    }
    return 0;
}

struct cst
{
    size_t j;
    size_t starter;
    int has;
    int adj;
    int last;
};

static struct cst cst_init(void)
{
    struct cst s;

    s.j = 0;
    s.starter = 0;
    s.has = 0;
    s.adj = 0;
    s.last = 0;
    return s;
}

static struct cst cst_starter(struct cst s)
{
    s.starter = s.j;
    s.has = 1;
    s.adj = 1;
    s.last = 0;
    return s;
}

static struct cst cst_mark(struct cst s, int cls)
{
    s.adj = 0;
    s.last = cls;
    return s;
}

static struct cst comp_emit(struct cst s, u89_cp *w, u89_cp ch, int cls)
{
    struct cst s0;

    s0 = s;
    w[s0.j] = ch;
    if (cls == 0)
    {
        s0 = cst_starter(s0);
    }
    else
    {
        s0 = cst_mark(s0, cls);
    }
    s0.j = s0.j + 1;
    return s0;
}

static struct cst comp_commit(struct cst s, u89_cp *w, u89_cp res)
{
    w[s.starter] = res;
    s.adj = 1;
    s.last = 0;
    return s;
}

static int comp_allowed(int adj, int last, int cls)
{
    if (adj)
    {
        return 1;
    }
    if (last < cls)
    {
        return 1;
    }
    return 0;
}

/* Process w[i] in one canonical-composition pass. Tries to compose it into the
   current starter when UAX #15's blocking rule permits (adjacent, or the last
   emitted scalar has a strictly smaller ccc), otherwise emits it. Generic table
   composition runs regardless of the second member's ccc, composing pairs such
   as U+09C7 + U+09BE -> U+09CB. Returns the advanced state. */
static struct cst compose_step(u89_cp *w, size_t i, struct cst s)
{
    u89_cp ch;
    int cls;
    int allowed;
    int done;
    u89_cp res;

    ch = w[i];
    cls = get_ccc(ch);
    allowed = 0;
    if (s.has)
    {
        allowed = comp_allowed(s.adj, s.last, cls);
    }
    done = 0;
    if (allowed)
    {
        done = compose_pair(w[s.starter], ch, &res);
    }
    if (done)
    {
        s = comp_commit(s, w, res);
    }
    if (!done)
    {
        s = comp_emit(s, w, ch, cls);
    }
    return s;
}

/* Apply one composition step at read index i, updating *s, and return i + 1. */
static size_t compose_iter(u89_cp *w, size_t i, struct cst *s)
{
    struct cst s1;

    s1 = compose_step(w, i, *s);
    *s = s1;
    return i + 1;
}

static size_t compose(u89_cp *w, size_t count)
{
    size_t i;
    struct cst s;

    s = cst_init();
    i = 0;
    while (i < count)
    {
        i = compose_iter(w, i, &s);
    }
    return s.j;
}

/* ---- Normalization driver ------------------------------------------------ */

/* Append the full decomposition of the scalar at *i into w (or count it when
   w is null). Advances *i past the scalar. Returns 0 on success, 1 on
   malformed UTF-8, 2 when the workspace is too small. */
static int decode_append(const unsigned char *s, size_t n, size_t *i,
                         int compat, u89_cp *w, size_t wcap, size_t *m)
{
    size_t next;
    u89_status st;
    int got;
    u89_cp cp;
    u89_cp tmp[32];

    st = u89_utf8_decode(s, n, *i, &cp, &next);
    if (st != U89_OK)
    {
        return 1;
    }
    got = decompose_one(cp, compat, tmp, 32);
    if (got < 0)
    {
        return 2;
    }
    if (w != NULL)
    {
        if (*m + (size_t)got > wcap)
        {
            return 2;
        }
    }
    *m = append_scalars(w, *m, tmp, (size_t)got);
    *i = next;
    return 0;
}

static int out_scan(const u89_cp *w, size_t m, size_t *i, size_t *total)
{
    int len;
    u89_cp cp;

    if (*i >= m)
    {
        return 0;
    }
    cp = w[*i];
    len = u89_utf8_len(cp);
    *total = *total + (size_t)len;
    *i = *i + 1;
    return 1;
}

static int form_compat(int mode)
{
    if (mode == 2)
    {
        return 1;
    }
    if (mode == 3)
    {
        return 1;
    }
    return 0;
}

static int form_compose(int mode)
{
    if (mode == 0)
    {
        return 1;
    }
    if (mode == 2)
    {
        return 1;
    }
    return 0;
}

/* Encode the next scalar w[*i] at dst[*off]; advances both. Returns 0 when all
   scalars are written. */
static int enc_forward(const u89_cp *w, size_t m, unsigned char *dst, size_t *i,
                       size_t *off)
{
    int len;
    u89_cp cp;

    if (*i >= m)
    {
        return 0;
    }
    cp = w[*i];
    len = u89_utf8_encode(cp, dst + *off);
    *off = *off + (size_t)len;
    *i = *i + 1;
    return 1;
}

static size_t out_bytes_of(const u89_cp *w, size_t m)
{
    size_t total;
    size_t i;
    int go;

    total = 0;
    i = 0;
    go = 1;
    while (go != 0)
    {
        go = out_scan(w, m, &i, &total);
    }
    return total;
}

static int mul_overflow(size_t a, size_t b)
{
    if (a != 0)
    {
        if (b > (size_t)-1 / a)
        {
            return 1;
        }
    }
    return 0;
}

int u89_normalize_work_bound(const unsigned char *s, size_t n, size_t *cp_bound)
{
    int v;
    int mo;

    if (cp_bound == NULL)
    {
        return U89_EINVAL;
    }
    v = u89_utf8_valid(s, n);
    if (v == 0)
    {
        return U89_EUTF8;
    }
    mo = mul_overflow(n, 18);
    if (mo)
    {
        return U89_EOVERFLOW;
    }
    *cp_bound = n * 18;
    return U89_OK;
}

int u89_normalize_out_bound(const unsigned char *s, size_t n,
                            size_t *byte_bound)
{
    int v;
    int mo;

    if (byte_bound == NULL)
    {
        return U89_EINVAL;
    }
    v = u89_utf8_valid(s, n);
    if (v == 0)
    {
        return U89_EUTF8;
    }
    mo = mul_overflow(n, 72);
    if (mo)
    {
        return U89_EOVERFLOW;
    }
    *byte_bound = n * 72;
    return U89_OK;
}

int u89_normalize_ex(int mode, const unsigned char *s, size_t n,
                     unsigned char *dst, size_t dst_cap, u89_cp *work,
                     size_t work_cap)
{
    size_t m;
    size_t i;
    size_t total;
    size_t off;
    int compat;
    int compose_on;
    int st;
    int go;

    if (mode < 0)
    {
        return U89_EINVAL;
    }
    if (mode > 3)
    {
        return U89_EINVAL;
    }
    if (n != 0)
    {
        if (work == NULL)
        {
            return U89_EINVAL;
        }
    }
    compat = form_compat(mode);
    compose_on = form_compose(mode);
    m = 0;
    i = 0;
    while (i < n)
    {
        st = decode_append(s, n, &i, compat, work, work_cap, &m);
        if (st == 1)
        {
            return U89_EUTF8;
        }
        if (st == 2)
        {
            return U89_EWORK;
        }
    }
    reorder(work, m);
    if (compose_on)
    {
        m = compose(work, m);
    }
    total = out_bytes_of(work, m);
    if (dst == NULL)
    {
        if (total > 0x7FFFFFFFUL)
        {
            return U89_ENOSPC;
        }
        return (int)total;
    }
    if (dst_cap < total)
    {
        return U89_ENOSPC;
    }
    off = 0;
    i = 0;
    go = 1;
    while (go != 0)
    {
        go = enc_forward(work, m, dst, &i, &off);
    }
    return (int)off;
}
