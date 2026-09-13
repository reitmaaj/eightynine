/* u89_grapheme.c - UAX #29 extended grapheme cluster boundaries.
 *
 * The forward scan and the backward candidate scan share the same basic
 * pair rules (GB3..GB9b, GB999). GB9c (Indic conjuncts), GB11 (emoji ZWJ),
 * and GB12/GB13 (regional indicators) carry context and are evaluated
 * separately: forward through a small state, backward through bounded
 * context jumps. No allocation is performed.
 */

#include "../include/u89.h"
#include "u89_tables.h"

#define GCB_OTHER 0
#define GCB_CR 1
#define GCB_LF 2
#define GCB_CONTROL 3
#define GCB_EXTEND 4
#define GCB_ZWJ 5
#define GCB_RI 6
#define GCB_PREPEND 7
#define GCB_SPACINGMARK 8
#define GCB_L 9
#define GCB_V 10
#define GCB_T 11
#define GCB_LV 12
#define GCB_LVT 13

#define INCB_NONE 0
#define INCB_LINKER 1
#define INCB_CONSONANT 2
#define INCB_EXTEND 3

typedef struct gc_state
{
    unsigned short ri_len;
    unsigned char emoji;
    unsigned char incb;
} gc_state;

/* ---- Property lookup ----------------------------------------------------- */

static int prop_probe(const u89_prop_range *t, size_t i, u89_cp cp,
                      unsigned short *out)
{
    if (cp < t[i].lo)
    {
        return 2;
    }
    if (cp > t[i].hi)
    {
        return 0;
    }
    *out = t[i].value;
    return 1;
}

static unsigned short prop_lookup(const u89_prop_range *t, size_t n, u89_cp cp,
                                  unsigned short dflt)
{
    size_t i;
    int st;
    unsigned short found;

    found = dflt;
    for (i = 0; i < n; ++i)
    {
        st = prop_probe(t, i, cp, &found);
        if (st == 2)
        {
            return dflt;
        }
        if (st == 1)
        {
            return found;
        }
    }
    return dflt;
}

static unsigned short gcb_at(u89_cp cp)
{
    unsigned short v;

    v = prop_lookup(u89_gcb_ranges, u89_gcb_count, cp, GCB_OTHER);
    return v;
}

static unsigned short incb_at(u89_cp cp)
{
    unsigned short v;

    v = prop_lookup(u89_incb_ranges, u89_incb_count, cp, INCB_NONE);
    return v;
}

static int range_probe(const u89_range *t, size_t i, u89_cp cp)
{
    if (cp < t[i].lo)
    {
        return 2;
    }
    if (cp <= t[i].hi)
    {
        return 1;
    }
    return 0;
}

static int range_member(const u89_range *t, size_t n, u89_cp cp)
{
    size_t i;
    int st;

    for (i = 0; i < n; ++i)
    {
        st = range_probe(t, i, cp);
        if (st == 1)
        {
            return 1;
        }
        if (st == 2)
        {
            return 0;
        }
    }
    return 0;
}

static int is_ext_pict(u89_cp cp)
{
    int v;

    v = range_member(u89_extpict_ranges, u89_extpict_count, cp);
    return v;
}

/* ---- Basic pair rules ---------------------------------------------------- */

static int is_control_class(unsigned short cls)
{
    if (cls == GCB_CONTROL)
    {
        return 1;
    }
    if (cls == GCB_CR)
    {
        return 1;
    }
    if (cls == GCB_LF)
    {
        return 1;
    }
    return 0;
}

static int hangul_no_break(unsigned short a, unsigned short b)
{
    if (a == GCB_L)
    {
        if (b == GCB_L)
        {
            return 1;
        }
        if (b == GCB_V)
        {
            return 1;
        }
        if (b == GCB_LV)
        {
            return 1;
        }
        if (b == GCB_LVT)
        {
            return 1;
        }
        return 0;
    }
    if (a == GCB_V)
    {
        if (b == GCB_V)
        {
            return 1;
        }
        if (b == GCB_T)
        {
            return 1;
        }
        return 0;
    }
    if (a == GCB_LV)
    {
        if (b == GCB_V)
        {
            return 1;
        }
        if (b == GCB_T)
        {
            return 1;
        }
        return 0;
    }
    if (a == GCB_LVT)
    {
        if (b == GCB_T)
        {
            return 1;
        }
        return 0;
    }
    if (a == GCB_T)
    {
        if (b == GCB_T)
        {
            return 1;
        }
        return 0;
    }
    return 0;
}

/* GB3..GB9b plus GB999: 0 when no break between prev and cur. */
static int basic_break(u89_cp prev, u89_cp cur)
{
    unsigned short a;
    unsigned short b;
    int ctrl;
    int hangul;

    a = gcb_at(prev);
    b = gcb_at(cur);
    if (a == GCB_CR)
    {
        if (b == GCB_LF)
        {
            return 0;
        }
    }
    ctrl = is_control_class(a);
    if (ctrl)
    {
        return 1;
    }
    ctrl = is_control_class(b);
    if (ctrl)
    {
        return 1;
    }
    hangul = hangul_no_break(a, b);
    if (hangul)
    {
        return 0;
    }
    if (b == GCB_EXTEND)
    {
        return 0;
    }
    if (b == GCB_ZWJ)
    {
        return 0;
    }
    if (b == GCB_SPACINGMARK)
    {
        return 0;
    }
    if (a == GCB_PREPEND)
    {
        return 0;
    }
    return 1;
}

/* ---- Forward scan -------------------------------------------------------- */

static unsigned short ri_advance(unsigned short ri_len, unsigned short cls)
{
    if (cls != GCB_RI)
    {
        return 0;
    }
    return (unsigned short)(ri_len + 1);
}

static unsigned char emoji_after_zwj(unsigned char emoji)
{
    unsigned char out;

    out = 0;
    if (emoji == 1)
    {
        out = 2;
    }
    return out;
}

/* An Extend keeps "Pictograph Extend*" alive, but it breaks the adjacency of
   a preceding ZWJ: GB11 requires the ZWJ immediately before the pictograph. */
static unsigned char emoji_after_extend(unsigned char emoji)
{
    if (emoji == 2)
    {
        return 0;
    }
    return emoji;
}

static unsigned char emoji_advance(unsigned char emoji, u89_cp cur)
{
    unsigned short cls;
    unsigned char out;
    int pict;

    cls = gcb_at(cur);
    out = 0;
    if (cls == GCB_EXTEND)
    {
        out = emoji_after_extend(emoji);
    }
    if (cls == GCB_ZWJ)
    {
        out = emoji_after_zwj(emoji);
    }
    pict = is_ext_pict(cur);
    if (pict)
    {
        out = 1;
    }
    return out;
}

static unsigned char incb_after_linker(unsigned char incb)
{
    unsigned char out;

    out = 0;
    if (incb != 0)
    {
        out = 2;
    }
    return out;
}

static unsigned char incb_advance(unsigned char incb, u89_cp cur)
{
    unsigned short ic;
    unsigned char out;

    ic = incb_at(cur);
    out = 0;
    if (ic == INCB_CONSONANT)
    {
        out = 1;
    }
    if (ic == INCB_EXTEND)
    {
        out = incb;
    }
    if (ic == INCB_LINKER)
    {
        out = incb_after_linker(incb);
    }
    return out;
}

static gc_state state_init(u89_cp cp)
{
    gc_state st;
    unsigned short cls;

    cls = gcb_at(cp);
    st.ri_len = ri_advance(0, cls);
    st.emoji = emoji_advance(0, cp);
    st.incb = incb_advance(0, cp);
    return st;
}

static gc_state state_advance(gc_state st, u89_cp cur)
{
    gc_state out;
    unsigned short cls;

    cls = gcb_at(cur);
    out.ri_len = ri_advance(st.ri_len, cls);
    out.emoji = emoji_advance(st.emoji, cur);
    out.incb = incb_advance(st.incb, cur);
    return out;
}

static int fwd_break(u89_cp prev, const gc_state *st, u89_cp cur)
{
    int br;
    int pict;
    unsigned short cls;

    br = basic_break(prev, cur);
    if (br == 0)
    {
        return 0;
    }
    cls = incb_at(cur);
    if (cls == INCB_CONSONANT)
    {
        if (st->incb == 2)
        {
            return 0;
        }
    }
    pict = is_ext_pict(cur);
    if (pict)
    {
        if (st->emoji == 2)
        {
            return 0;
        }
    }
    cls = gcb_at(prev);
    if (cls == GCB_RI)
    {
        cls = gcb_at(cur);
        if (cls == GCB_RI)
        {
            if (st->ri_len % 2 == 1)
            {
                return 0;
            }
        }
    }
    return 1;
}

/* Process one scalar. Returns 0 to continue, 1 at a break, 2 at end. */
static int scan_step(const unsigned char *s, size_t n, size_t *i, u89_cp *prev,
                     gc_state *state)
{
    size_t next;
    u89_cp cur;
    u89_status st;
    int br;

    if (*i >= n)
    {
        return 2;
    }
    st = u89_utf8_decode(s, n, *i, &cur, &next);
    if (st != U89_OK)
    {
        return 2;
    }
    br = fwd_break(*prev, state, cur);
    if (br != 0)
    {
        return 1;
    }
    *state = state_advance(*state, cur);
    *prev = cur;
    *i = next;
    return 0;
}

/* Next boundary strictly after the cluster starting at start (or n). */
static size_t scan_next(const unsigned char *s, size_t n, size_t start)
{
    size_t i;
    size_t next;
    u89_cp prev;
    u89_status st;
    gc_state state;
    int r;
    int go;

    if (start >= n)
    {
        return n;
    }
    st = u89_utf8_decode(s, n, start, &prev, &next);
    if (st != U89_OK)
    {
        return n;
    }
    state = state_init(prev);
    i = next;
    r = 0;
    go = 1;
    while (go != 0)
    {
        r = scan_step(s, n, &i, &prev, &state);
        if (r != 0)
        {
            go = 0;
        }
    }
    if (r == 1)
    {
        return i;
    }
    return n;
}

/* ---- Backward scan ------------------------------------------------------- */

/* One basic pair step backwards. Returns 1 when *start moved. */
static int back_step(const unsigned char *s, size_t n, size_t *start,
                     u89_cp *cur)
{
    size_t q;
    u89_cp prev;
    u89_status st;
    int br;

    if (*start == 0)
    {
        return 0;
    }
    st = u89_utf8_prev(s, n, *start, &prev, &q);
    if (st != U89_OK)
    {
        return 0;
    }
    br = basic_break(prev, *cur);
    if (br != 0)
    {
        return 0;
    }
    *start = q;
    *cur = prev;
    return 1;
}

static void incb_mark_linker(size_t *i, size_t q, int *linker)
{
    *i = q;
    *linker = 1;
}

static int incb_take(size_t *i, size_t q, int *linker, unsigned short cls)
{
    if (cls == INCB_EXTEND)
    {
        *i = q;
        return 1;
    }
    if (cls == INCB_LINKER)
    {
        incb_mark_linker(i, q, linker);
        return 1;
    }
    return 0;
}

/* One backward GB9c run step. Returns 1 when *i moved, 0 when the run ended
   (with *cp and *q at the scalar that stopped it). */
static int incb_step(const unsigned char *s, size_t n, size_t *i, u89_cp *cp,
                     size_t *q, int *linker)
{
    u89_status st;
    unsigned short cls;
    int r;

    st = u89_utf8_prev(s, n, *i, cp, q);
    if (st != U89_OK)
    {
        return 0;
    }
    cls = incb_at(*cp);
    r = incb_take(i, *q, linker, cls);
    return r;
}

/* GB9c: jump to the left consonant of Consonant (Extend|Linker)* Linker
   (Extend|Linker)* [start], or return start. */
static size_t incb_jump(const unsigned char *s, size_t n, size_t start)
{
    size_t i;
    size_t q;
    u89_cp cp;
    u89_status st;
    unsigned short cls;
    int linker;
    int r;

    st = u89_utf8_decode(s, n, start, &cp, &q);
    if (st != U89_OK)
    {
        return start;
    }
    cls = incb_at(cp);
    if (cls != INCB_CONSONANT)
    {
        return start;
    }
    i = start;
    linker = 0;
    while (i > 0)
    {
        r = incb_step(s, n, &i, &cp, &q, &linker);
        if (r == 0)
        {
            break;
        }
    }
    if (linker == 0)
    {
        return start;
    }
    cls = incb_at(cp);
    if (cls != INCB_CONSONANT)
    {
        return start;
    }
    return q;
}

/* GB11: jump to the left pictograph of Pictograph Extend* ZWJ [start], or
   return start. */
static size_t gb11_jump(const unsigned char *s, size_t n, size_t start)
{
    size_t i;
    size_t q;
    u89_cp cp;
    u89_status st;
    unsigned short cls;
    int pict;

    st = u89_utf8_decode(s, n, start, &cp, &q);
    if (st != U89_OK)
    {
        return start;
    }
    pict = is_ext_pict(cp);
    if (!pict)
    {
        return start;
    }
    if (start == 0)
    {
        return start;
    }
    st = u89_utf8_prev(s, n, start, &cp, &q);
    if (st != U89_OK)
    {
        return start;
    }
    cls = gcb_at(cp);
    if (cls != GCB_ZWJ)
    {
        return start;
    }
    i = q;
    while (i > 0)
    {
        st = u89_utf8_prev(s, n, i, &cp, &q);
        if (st != U89_OK)
        {
            return start;
        }
        cls = gcb_at(cp);
        if (cls != GCB_EXTEND)
        {
            break;
        }
        i = q;
    }
    if (i == 0)
    {
        return start;
    }
    pict = is_ext_pict(cp);
    if (!pict)
    {
        return start;
    }
    return q;
}

static void ri_take(size_t *i, size_t *count, size_t q, size_t *first)
{
    if (*count == 0)
    {
        *first = q;
    }
    *i = q;
    *count = *count + 1;
}

/* GB12/GB13: when an odd run of regional indicators precedes start, jump to
   the first indicator of the pair (the run's last indicator); otherwise
   return start. */
static size_t ri_jump(const unsigned char *s, size_t n, size_t start)
{
    size_t i;
    size_t q;
    size_t count;
    size_t first;
    u89_cp cp;
    u89_status st;
    unsigned short cls;

    st = u89_utf8_decode(s, n, start, &cp, &q);
    if (st != U89_OK)
    {
        return start;
    }
    cls = gcb_at(cp);
    if (cls != GCB_RI)
    {
        return start;
    }
    i = start;
    count = 0;
    first = start;
    while (i > 0)
    {
        st = u89_utf8_prev(s, n, i, &cp, &q);
        if (st != U89_OK)
        {
            return start;
        }
        cls = gcb_at(cp);
        if (cls != GCB_RI)
        {
            break;
        }
        ri_take(&i, &count, q, &first);
    }
    if (count % 2 == 0)
    {
        return start;
    }
    return first;
}

static size_t min_start(size_t a, size_t b)
{
    if (b < a)
    {
        return b;
    }
    return a;
}

/* One context jump backwards. Returns 1 when *start moved. */
static int extend_step(const unsigned char *s, size_t n, size_t *start,
                       u89_cp *cur)
{
    size_t best;
    size_t a;
    size_t b;
    size_t c;
    size_t next;
    u89_cp cp;
    u89_status st;

    best = *start;
    a = incb_jump(s, n, *start);
    best = min_start(best, a);
    b = gb11_jump(s, n, *start);
    best = min_start(best, b);
    c = ri_jump(s, n, *start);
    best = min_start(best, c);
    if (best == *start)
    {
        return 0;
    }
    st = u89_utf8_decode(s, n, best, &cp, &next);
    if (st != U89_OK)
    {
        return 0;
    }
    *start = best;
    *cur = cp;
    return 1;
}

/* One backward move: basic pair first, then a context jump. */
static int cluster_step(const unsigned char *s, size_t n, size_t *start,
                        u89_cp *cur)
{
    int moved;

    moved = back_step(s, n, start, cur);
    if (moved != 0)
    {
        return 1;
    }
    moved = extend_step(s, n, start, cur);
    return moved;
}

/* Start of the cluster containing the scalar at scalar start p. */
static size_t cluster_start(const unsigned char *s, size_t n, size_t p)
{
    size_t start;
    size_t next;
    u89_cp cur;
    u89_status st;
    int go;

    st = u89_utf8_decode(s, n, p, &cur, &next);
    if (st != U89_OK)
    {
        return p;
    }
    start = p;
    go = 1;
    while (go != 0)
    {
        go = cluster_step(s, n, &start, &cur);
    }
    return start;
}

/* Start of the scalar containing byte pos (scanning back at most four bytes
   over continuation bytes). */
static size_t lead_floor(size_t pos)
{
    if (pos > 4)
    {
        return pos - 4;
    }
    return 0;
}

static int lead_step(const unsigned char *s, size_t *i, size_t floor)
{
    if (*i <= floor)
    {
        return 0;
    }
    if ((s[*i] & 0xC0) != 0x80)
    {
        return 0;
    }
    *i = *i - 1;
    return 1;
}

static size_t lead_start(const unsigned char *s, size_t pos)
{
    size_t i;
    size_t floor;
    int go;

    i = pos;
    floor = lead_floor(pos);
    go = 1;
    while (go != 0)
    {
        go = lead_step(s, &i, floor);
    }
    return i;
}

/* ---- Public interface ---------------------------------------------------- */

size_t u89_grapheme_next(const unsigned char *s, size_t n, size_t pos)
{
    size_t q;
    size_t c;

    if (pos >= n)
    {
        return n;
    }
    q = lead_start(s, pos);
    c = cluster_start(s, n, q);
    q = scan_next(s, n, c);
    return q;
}

size_t u89_grapheme_prev(const unsigned char *s, size_t n, size_t pos)
{
    size_t q;

    if (pos == 0)
    {
        return 0;
    }
    q = lead_start(s, pos - 1);
    q = cluster_start(s, n, q);
    return q;
}

int u89_grapheme_boundary(const unsigned char *s, size_t n, size_t pos)
{
    size_t q;
    size_t c;
    size_t end;
    u89_cp cp;
    u89_status st;

    if (pos == 0)
    {
        return 1;
    }
    if (pos > n)
    {
        return 0;
    }
    if (pos == n)
    {
        return 1;
    }
    st = u89_utf8_prev(s, n, pos, &cp, &q);
    if (st != U89_OK)
    {
        return 0;
    }
    c = cluster_start(s, n, q);
    end = scan_next(s, n, c);
    if (end == pos)
    {
        return 1;
    }
    return 0;
}
