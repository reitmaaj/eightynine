/* u89_width.c - terminal display width and Unicode-whitespace word wrap. */

#include "../include/u89.h"
#include "u89_tables.h"

/* Probe sorted, non-overlapping width ranges. Returns 0 to continue, 2 when
   cp precedes range i (stop scanning), or the width type plus 3 when cp lies
   in range i. */
static int width_probe(const u89_wrange *t, size_t i, u89_cp cp);

static int width_probe(const u89_wrange *t, size_t i, u89_cp cp)
{
    u89_cp lo;
    u89_cp hi;

    lo = t[i].lo;
    if (cp < lo)
    {
        return 2;
    }
    hi = t[i].hi;
    if (cp > hi)
    {
        return 0;
    }
    return t[i].type + 3;
}

static int width_type_at(u89_cp cp)
{
    size_t i;
    int code;

    for (i = 0; i < u89_width_count; ++i)
    {
        code = width_probe(u89_width_ranges, i, cp);
        if (code == 2)
        {
            return 4;
        }
        if (code != 0)
        {
            return code - 3;
        }
    }
    return 4;
}

int u89_width(u89_cp cp, u89_ambig a)
{
    int sc;
    int t;

    sc = u89_is_scalar(cp);
    if (!sc)
    {
        return -1;
    }
    t = width_type_at(cp);
    if (t == 0)
    {
        return -1;
    }
    if (t == 1)
    {
        return 0;
    }
    if (t == 2)
    {
        return 2;
    }
    if (t == 3)
    {
        if (a == U89_AMBIG_WIDE)
        {
            return 2;
        }
        return 1;
    }
    return 1;
}

/* Display width of cp in cells, clamping control/unprintable scalars to 0. */
static int cp_disp_width(u89_cp cp, u89_ambig a)
{
    int w;

    w = u89_width(cp, a);
    if (w < 0)
    {
        w = 0;
    }
    return w;
}

/* Advance past one non-whitespace scalar, adding its width to *w and its byte
   length to *i. Returns 1 when a printable non-whitespace scalar was consumed;
   returns 0 at end of input, on whitespace, or on a control, leaving *i and
   *w unchanged. */
static int word_next(const unsigned char *s, size_t n, u89_ambig a, size_t *i,
                     int *w)
{
    size_t next;
    u89_status st;
    int ws;
    int cw;
    u89_cp cp;

    if (*i >= n)
    {
        return 0;
    }
    st = u89_utf8_decode(s, n, *i, &cp, &next);
    if (st != U89_OK)
    {
        return 0;
    }
    ws = u89_is_whitespace(cp);
    if (ws)
    {
        return 0;
    }
    cw = cp_disp_width(cp, a);
    *w = *w + cw;
    *i = next;
    return 1;
}

/* Measure the maximal non-whitespace run at s. Stores its byte length in
 *byte_len and its display width in *width. */
static void word_measure(const unsigned char *s, size_t n, u89_ambig a,
                         size_t *byte_len, int *width)
{
    size_t i;
    int w;
    int go;

    i = 0;
    w = 0;
    go = 1;
    while (go != 0)
    {
        go = word_next(s, n, a, &i, &w);
    }
    *byte_len = i;
    *width = w;
}

/* Return 1 when col > 0 and col + ww exceeds max_cols (a break is needed
   before this word). */
static int need_break(int col, int ww, int max_cols)
{
    if (col <= 0)
    {
        return 0;
    }
    if (col + ww <= max_cols)
    {
        return 0;
    }
    return 1;
}

/* Record a break offset at i (bounded by cap) and reset the current column. */
static void apply_break(size_t *breaks, size_t cap, size_t i, size_t *break_i,
                        int *col)
{
    if (*break_i < cap)
    {
        breaks[*break_i] = i;
    }
    *break_i = *break_i + 1;
    *col = 0;
}

/* Consume a single whitespace scalar of width cp_disp_width(cp). */
static void ws_advance(u89_cp cp, size_t next, u89_ambig a, size_t *i, int *col)
{
    int w;

    w = cp_disp_width(cp, a);
    *col = *col + w;
    *i = next;
}

/* Process one logical token starting at *i: a whitespace scalar or a whole
   non-whitespace word. Records a break before an overflowing word. Returns 1
   while input remains, 0 at end of input or on malformed UTF-8. */
static int wrap_step(const unsigned char *s, size_t n, int max_cols,
                     u89_ambig a, size_t *breaks, size_t cap, size_t *i,
                     size_t *break_i, int *col)
{
    size_t next;
    u89_status st;
    int ws;
    int ww;
    int emit;
    size_t wlen;
    u89_cp cp;

    st = u89_utf8_decode(s, n, *i, &cp, &next);
    if (st != U89_OK)
    {
        return 0;
    }
    ws = u89_is_whitespace(cp);
    if (ws)
    {
        ws_advance(cp, next, a, i, col);
        return 1;
    }
    word_measure(s + *i, n - *i, a, &wlen, &ww);
    emit = need_break(*col, ww, max_cols);
    if (emit != 0)
    {
        apply_break(breaks, cap, *i, break_i, col);
    }
    *col = *col + ww;
    *i = *i + wlen;
    return 1;
}

int u89_width_wrap(const unsigned char *s, size_t n, int max_cols, u89_ambig a,
                   size_t *breaks, size_t cap)
{
    size_t i;
    size_t break_i;
    int col;
    int go;

    i = 0;
    break_i = 0;
    col = 0;
    go = 1;
    while (go != 0)
    {
        go = wrap_step(s, n, max_cols, a, breaks, cap, &i, &break_i, &col);
    }
    return (int)break_i;
}
