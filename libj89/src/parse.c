/* parse.c - recursive-descent JSON parser for libj89. */
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "u89.h"

/* Exact integer representability limit for `double`: every integer |n| <= 2^53
 * is exact. Integer tokens beyond this range are rejected (not approximated).
 * 2^53 = 9007199254740992, which fits a 64-bit `long` accumulator. */
#define J89_INT_MAX 9007199254740992L
#define J89_INT_MIN (-9007199254740992L)

struct j89_parser
{
    const char *buf;
    j89_len len;
    j89_len pos;
    j89_arena *a;
    int ok;
    j89_len depth; /* current array/object nesting depth */
};

struct j89_pmember
{
    j89_len ko;
    j89_len kl;
    j89_len value;
};

static int j89_key_eq(j89_arena *a, j89_len ko1, j89_len kl1, j89_len ko2,
                      j89_len kl2)
{
    const char *s1;
    const char *s2;
    j89_len i;
    if (kl1 != kl2)
    {
        return 0;
    }
    s1 = j89_str_bytes(a, ko1);
    s2 = j89_str_bytes(a, ko2);
    for (i = 0; i < kl1; i = i + 1)
    {
        if (s1[i] != s2[i])
        {
            return 0;
        }
    }
    return 1;
}

static int j89_is_ws(int c)
{
    int r;
    r = 0;
    if (c == ' ')
    {
        r = 1;
    }
    if (c == '\t')
    {
        r = 1;
    }
    if (c == '\n')
    {
        r = 1;
    }
    if (c == '\r')
    {
        r = 1;
    }
    return r;
}

static int j89_is_digit(int c)
{
    int r;
    r = 0;
    if (c >= '0')
    {
        if (c <= '9')
        {
            r = 1;
        }
    }
    return r;
}

static int j89_is_control(int c)
{
    int r;
    int neg;
    r = 0;
    neg = (c < 0);
    if (!neg)
    {
        if (c < 0x20)
        {
            r = 1;
        }
    }
    return r;
}

static int j89_cur(struct j89_parser *p)
{
    if (p->pos >= p->len)
    {
        return -1;
    }
    return p->buf[p->pos];
}

static int j89_eof(struct j89_parser *p)
{
    j89_len i;
    j89_len len;
    int e;
    i = p->pos;
    len = p->len;
    e = (i >= len);
    return e;
}

static void j89_bump(struct j89_parser *p)
{
    j89_len pos;
    j89_len npos;
    pos = p->pos;
    npos = pos + 1;
    p->pos = npos;
}

/* Consume one whitespace character. Returns 1 when whitespace was consumed
 * (so skipping continues), 0 when the current character is not whitespace. */
static int j89_ws_step(struct j89_parser *p)
{
    int c;
    int w;
    c = j89_cur(p);
    w = j89_is_ws(c);
    if (w != 0)
    {
        j89_bump(p);
        return 1;
    }
    return 0;
}

static void j89_skip_ws(struct j89_parser *p)
{
    int go;
    go = 1;
    while (go != 0)
    {
        go = j89_ws_step(p);
    }
}

static void j89_fail(struct j89_parser *p, const char *msg)
{
    j89_set_error(p->a, msg);
    p->ok = 0;
}

static int j89_hexval(int c)
{
    if (c >= '0')
    {
        if (c <= '9')
        {
            return c - '0';
        }
    }
    if (c >= 'a')
    {
        if (c <= 'f')
        {
            return c - 'a' + 10;
        }
    }
    if (c >= 'A')
    {
        if (c <= 'F')
        {
            return c - 'A' + 10;
        }
    }
    return -1;
}

/* Consume one hex digit at the current position, folding it into *v. Returns
 * 0 on success, nonzero (with an error recorded) when the character is not a
 * hex digit. */
static int j89_hex_digit(struct j89_parser *p, unsigned int *v)
{
    int c;
    int h;
    unsigned int u;
    c = j89_cur(p);
    h = j89_hexval(c);
    if (h < 0)
    {
        j89_fail(p, "invalid hex escape");
        return 1;
    }
    u = (unsigned int)h;
    *v = *v * 16 + u;
    j89_bump(p);
    return 0;
}

static int j89_parse_hex4(struct j89_parser *p, unsigned int *out)
{
    unsigned int v;
    int i;
    v = 0;
    for (i = 0; i < 4; i = i + 1)
    {
        int step;
        step = j89_hex_digit(p, &v);
        if (step != 0)
        {
            return 0;
        }
    }
    *out = v;
    return 1;
}

/* Append raw bytes to the string builder, recording an out-of-memory error.
 * Returns 0 on success, -1 on failure. */
static int j89_append_bytes(struct j89_parser *p, str89_buf *b,
                            const unsigned char *src, size_t n)
{
    str89_view v;
    int r;
    v.data = src;
    v.len = n;
    r = str89_buf_append(b, NULL, v);
    if (r != STR89_OK)
    {
        j89_fail(p, "out of memory");
        return -1;
    }
    return 0;
}

/* Validate and copy one UTF-8 sequence beginning at the current position.
 * Returns 1 when a sequence was copied, 0 on invalid UTF-8, -1 on allocation
 * failure (already recorded). Scalar validity (overlong, surrogate, range,
 * truncation) is owned by libu89; the original bytes are copied, not
 * re-encoded. */
static int j89_utf8_seq(struct j89_parser *p, str89_buf *b)
{
    const unsigned char *s;
    size_t next;
    size_t n;
    u89_status st;
    int r;
    s = (const unsigned char *)p->buf;
    next = 0;
    st = u89_utf8_decode(s, p->len, p->pos, NULL, &next);
    if (st != U89_OK)
    {
        return 0;
    }
    n = next - p->pos;
    r = j89_append_bytes(p, b, s + p->pos, n);
    if (r != 0)
    {
        return -1;
    }
    p->pos = next;
    return 1;
}

/* Append one decoded byte value to the string builder. */
static int j89_str_emit_char(struct j89_parser *p, str89_buf *b,
                             unsigned int val)
{
    unsigned char uc;
    int r;
    uc = (unsigned char)val;
    r = j89_append_bytes(p, b, &uc, 1);
    return r;
}

/* Append one Unicode scalar value to the string builder. */
static int j89_str_emit_cp(struct j89_parser *p, str89_buf *b, u89_cp cp)
{
    int r;
    r = str89_buf_append_cp(b, NULL, cp);
    if (r != STR89_OK)
    {
        j89_fail(p, "out of memory");
        return -1;
    }
    return 0;
}

/* Combine a high surrogate `unit` with a required following low surrogate
 * escape. The '\', 'u', and four hex digits of the low surrogate are
 * consumed. Returns 0 on success (unit updated to the supplementary scalar),
 * 1 on error. */
static int j89_str_pair(struct j89_parser *p, unsigned int *unit)
{
    unsigned int lo;
    u89_cp cp;
    int ok;
    int r;
    int c2;
    c2 = j89_cur(p);
    if (c2 != '\\')
    {
        j89_fail(p, "lone high surrogate");
        return 1;
    }
    j89_bump(p);
    c2 = j89_cur(p);
    if (c2 != 'u')
    {
        j89_fail(p, "lone high surrogate");
        return 1;
    }
    j89_bump(p);
    r = j89_parse_hex4(p, &lo);
    if (r == 0)
    {
        return 1;
    }
    cp = 0;
    ok = u89_utf16_decode_pair(*unit, lo, &cp);
    if (ok == 0)
    {
        j89_fail(p, "bad low surrogate");
        return 1;
    }
    *unit = cp;
    return 0;
}

/* Consume the four hex digits of a \u escape at the current position (the
 * backslash and 'u' are already consumed) and resolve surrogate pairing.
 * Stores the decoded code point. Returns 0 on success, nonzero on error. */
static int j89_str_read_cp(struct j89_parser *p, unsigned int *unit)
{
    int high;
    int low;
    int st;
    int r;
    r = j89_parse_hex4(p, unit);
    if (r == 0)
    {
        return 1;
    }
    high = u89_utf16_is_high_surrogate(*unit);
    if (high != 0)
    {
        st = j89_str_pair(p, unit);
        return st;
    }
    low = u89_utf16_is_low_surrogate(*unit);
    if (low != 0)
    {
        j89_fail(p, "lone low surrogate");
        return 1;
    }
    return 0;
}

/* Handle a \u escape (backslash and 'u' already consumed). */
static int j89_str_unicode(struct j89_parser *p, str89_buf *b)
{
    unsigned int cp;
    int st;
    int ok;
    st = j89_str_read_cp(p, &cp);
    if (st != 0)
    {
        return -1;
    }
    ok = j89_str_emit_cp(p, b, cp);
    return ok;
}

/* Map a simple (non-\u) escape character to its decoded byte, or -1 when it
 * is not a valid simple escape. */
static int j89_str_simple_val(int esc)
{
    if (esc == 'n')
    {
        return '\n';
    }
    if (esc == 't')
    {
        return '\t';
    }
    if (esc == 'r')
    {
        return '\r';
    }
    if (esc == 'b')
    {
        return '\b';
    }
    if (esc == 'f')
    {
        return '\f';
    }
    if (esc == '\\')
    {
        return '\\';
    }
    if (esc == '"')
    {
        return '"';
    }
    if (esc == '/')
    {
        return '/';
    }
    return -1;
}

/* Dispatch an escape (backslash and escape char already consumed). */
static int j89_str_dispatch(struct j89_parser *p, str89_buf *b, int esc)
{
    int val;
    int st;
    if (esc == 'u')
    {
        st = j89_str_unicode(p, b);
        return st;
    }
    val = j89_str_simple_val(esc);
    if (val < 0)
    {
        j89_fail(p, "invalid escape");
        return -1;
    }
    st = j89_str_emit_char(p, b, (unsigned int)val);
    return st;
}

/* Handle a backslash escape beginning at the current position. */
static int j89_str_escape(struct j89_parser *p, str89_buf *b)
{
    int esc;
    int st;
    j89_bump(p); /* consume the backslash */
    esc = j89_cur(p);
    j89_bump(p); /* consume the escape character */
    st = j89_str_dispatch(p, b, esc);
    return st;
}

/* Consume a multi-byte UTF-8 sequence and append it to the string. */
static int j89_str_utf8_emit(struct j89_parser *p, str89_buf *b)
{
    int r;
    r = j89_utf8_seq(p, b);
    if (r == 0)
    {
        j89_fail(p, "invalid UTF-8 in string");
        return -1;
    }
    if (r < 0)
    {
        return -1;
    }
    return 0;
}

/* Append one non-quote, non-backslash character (byte or UTF-8 sequence). */
static int j89_str_byte(struct j89_parser *p, str89_buf *b)
{
    int c;
    unsigned char uc;
    int st;
    c = j89_cur(p);
    uc = (unsigned char)c;
    if (uc >= 0x80)
    {
        st = j89_str_utf8_emit(p, b);
        return st;
    }
    st = j89_append_bytes(p, b, &uc, 1);
    if (st != 0)
    {
        return -1;
    }
    j89_bump(p);
    return 0;
}

/* Handle a raw (unquoted, unescaped) character of the string body. */
static int j89_str_raw(struct j89_parser *p, str89_buf *b)
{
    int c;
    int ctl;
    int st;
    c = j89_cur(p);
    ctl = j89_is_control(c);
    if (ctl != 0)
    {
        j89_fail(p, "control character in string");
        return -1;
    }
    st = j89_str_byte(p, b);
    return st;
}

/* Handle one character of the string body at the current position. Returns
 * 0 to keep scanning, 1 when the closing quote was consumed, -1 on a fatal
 * parse error (already recorded). */
static int j89_str_step(struct j89_parser *p, str89_buf *b)
{
    int e;
    int c;
    int st;
    e = j89_eof(p);
    if (e != 0)
    {
        j89_fail(p, "unterminated string");
        return -1;
    }
    c = j89_cur(p);
    if (c == '"')
    {
        j89_bump(p);
        return 1;
    }
    if (c == '\\')
    {
        st = j89_str_escape(p, b);
        return st;
    }
    st = j89_str_raw(p, b);
    return st;
}

/* Free the partial builder, record out-of-memory, and return the bad node
 * sentinel. */
static j89_len j89_string_buf_fail(struct j89_parser *p, str89_buf *b)
{
    str89_buf_free(b, NULL);
    j89_fail(p, "out of memory");
    return J89_BAD;
}

/* Free an owned string the arena did not adopt, record out-of-memory, and
 * return the bad node sentinel. */
static j89_len j89_string_owned_fail(struct j89_parser *p, str89 *owned)
{
    str89_free(owned, NULL);
    j89_fail(p, "out of memory");
    return J89_BAD;
}

static j89_len j89_parse_string(struct j89_parser *p)
{
    j89_arena *a;
    str89_buf b;
    str89 owned;
    j89_len node;
    int bad;
    int st;
    int r;
    a = p->a;
    str89_buf_init(&b);
    j89_bump(p); /* consume opening quote */
    for (;;)
    {
        st = j89_str_step(p, &b);
        if (st == 1)
        {
            break;
        }
        if (st == -1)
        {
            str89_buf_free(&b, NULL);
            return J89_BAD;
        }
    }
    r = str89_buf_reserve(&b, NULL, b.len + 1);
    if (r != STR89_OK)
    {
        node = j89_string_buf_fail(p, &b);
        return node;
    }
    b.data[b.len] = '\0';
    str89_init(&owned);
    r = str89_take(&owned, &b);
    if (r != STR89_OK)
    {
        node = j89_string_buf_fail(p, &b);
        return node;
    }
    node = j89_string_node(a, &owned);
    bad = j89_is_bad(node);
    if (bad)
    {
        node = j89_string_owned_fail(p, &owned);
        return node;
    }
    return node;
}

/* Accumulate one exponent digit into *e, clamped to bound. Returns the new
 * exponent value. */
static long j89_clamp_acc(long e, int digit, long bound)
{
    if (e > bound / 10)
    {
        return bound;
    }
    if (e * 10 + digit > bound)
    {
        return bound;
    }
    return e * 10 + digit;
}

/* Consume one exponent digit at the current position. Returns 1 when the
 * current character was a digit (folded into *e), 0 otherwise. */
static int j89_exp_digit_step(struct j89_parser *p, long *e, long bound,
                              int *any, int *c)
{
    int dg;
    int digit;
    long ne;
    dg = j89_is_digit(*c);
    if (dg == 0)
    {
        return 0;
    }
    digit = *c - '0';
    *any = 1;
    ne = j89_clamp_acc(*e, digit, bound);
    *e = ne;
    j89_bump(p);
    *c = j89_cur(p);
    return 1;
}

/* Consume all exponent digits, folding each into *e (clamped). */
static void j89_scan_exp_digits(struct j89_parser *p, long *e, long bound,
                                int *any, int *c)
{
    int cont;
    cont = 1;
    while (cont != 0)
    {
        cont = j89_exp_digit_step(p, e, bound, any, c);
    }
}

static void j89_exp_plus(struct j89_parser *p, int *esign, int *outc)
{
    j89_bump(p);
    *esign = 1;
    *outc = j89_cur(p);
}

static void j89_exp_minus(struct j89_parser *p, int *esign, int *outc)
{
    j89_bump(p);
    *esign = 2;
    *outc = j89_cur(p);
}

/* Consume an optional exponent sign. *esign: 0 none, 1 '+', 2 '-'. Sets
 * *outc to the current character after any sign is consumed. */
static void j89_exp_advance(struct j89_parser *p, int *esign, int *outc)
{
    int c;
    c = j89_cur(p);
    if (c == '+')
    {
        j89_exp_plus(p, esign, outc);
        return;
    }
    if (c == '-')
    {
        j89_exp_minus(p, esign, outc);
        return;
    }
    *esign = 0;
    *outc = c;
}

/* Parse a JSON exponent beginning at 'e'/'E'. Advances past the exponent and
 * stores it in *out, clamped to avoid overflow. Returns 1 on success. */
static int j89_parse_exp(struct j89_parser *p, long *out)
{
    long e;
    int any;
    long bound;
    int esign;
    int c;
    e = 0;
    any = 0;
    bound = 1073741824; /* 2^30, far beyond the double range */
    j89_bump(p);        /* consume the 'e'/'E' */
    j89_exp_advance(p, &esign, &c);
    j89_scan_exp_digits(p, &e, bound, &any, &c);
    if (any == 0)
    {
        j89_fail(p, "invalid number");
        return 0;
    }
    if (esign == 2)
    {
        e = -e;
    }
    *out = e;
    return 1;
}

/* Append the mantissa digit at buf[tok0+i] unless it is '-' or '.'. Updates
 * the copy cursor *ti and the all-zero flag *nonzero. */
static void j89_collect_mant(struct j89_parser *p, char *tmp, j89_len tok0,
                             j89_len i, j89_len *ti, int *nonzero)
{
    char ch;
    const char *buf;
    buf = p->buf;
    ch = buf[tok0 + i];
    if (ch == '-')
    {
        return;
    }
    if (ch == '.')
    {
        return;
    }
    tmp[*ti] = ch;
    *ti = *ti + 1;
    if (ch != '0')
    {
        *nonzero = 1;
    }
}

/* Copy exponent character expbuf[k] into tmp[base + k]. */
static void j89_copy_exp_char(char *tmp, j89_len base, const char *expbuf,
                              int k)
{
    char ec;
    j89_len kl;
    j89_len idx;
    ec = expbuf[k];
    kl = (j89_len)k;
    idx = base + kl;
    tmp[idx] = ec;
}

/* Convert a validated number token (span [tok0,tok1) in the input, minus a
 * possible leading '-') into a double using strtod. The token is rebuilt as
 * an integer digit string followed by an explicit exponent, so no decimal
 * point is passed to strtod (avoids locale-dependent parsing) while keeping
 * full input precision and correctly-rounded results. Returns 0 on success,
 * 1 on out-of-range (overflow/underflow), -1 on allocation failure. */
static int j89_token_to_double(struct j89_parser *p, j89_len tok0, j89_len tok1,
                               long e, int frac, double *out)
{
    j89_len len;
    j89_len cap;
    char *tmp;
    void *mv;
    j89_len ti;
    j89_len i;
    j89_len base;
    long net;
    long fl;
    int nonzero;
    int sl;
    int k;
    char expbuf[32];
    const char *cts;
    double v;
    len = tok1 - tok0;
    cap = len + 64;
    mv = malloc(cap);
    if (!mv)
    {
        return -1;
    }
    tmp = (char *)mv;
    ti = 0;
    nonzero = 0;
    for (i = 0; i < len; i = i + 1)
    {
        j89_collect_mant(p, tmp, tok0, i, &ti, &nonzero);
    }
    tmp[ti] = 'e';
    fl = (long)frac;
    net = e - fl;
    sl = sprintf(expbuf, "%ld", net);
    base = ti + 1;
    for (k = 0; k < sl; k = k + 1)
    {
        j89_copy_exp_char(tmp, base, expbuf, k);
    }
    tmp[base + (j89_len)sl] = '\0';
    cts = (const char *)tmp;
    v = strtod(cts, NULL);
    free(tmp);
    if (v != v)
    {
        return 1;
    }
    if (v > DBL_MAX)
    {
        return 1;
    }
    if (v < -DBL_MAX)
    {
        return 1;
    }
    if (v == 0.0)
    {
        if (nonzero)
        {
            return 1;
        }
    }
    *out = v;
    return 0;
}

static int j89_take_neg(struct j89_parser *p)
{
    int c;
    c = j89_cur(p);
    if (c == '-')
    {
        j89_bump(p);
        return 1;
    }
    return 0;
}

/* Accumulate a positive integer digit into *ival, detecting overflow. */
static void j89_acc_pos(long *ival, int *iover, int digit)
{
    long lim;
    lim = (J89_INT_MAX - digit) / 10;
    if (*ival > lim)
    {
        *iover = 1;
        return;
    }
    *ival = *ival * 10 + digit;
}

/* Accumulate a negative integer digit into *ival, detecting overflow. */
static void j89_acc_neg(long *ival, int *iover, int digit)
{
    long lim;
    lim = (J89_INT_MIN + digit) / 10;
    if (*ival < lim)
    {
        *iover = 1;
        return;
    }
    *ival = *ival * 10 - digit;
}

/* Consume one integer digit at the current position (neg gives the sign for
 * the running accumulator). Returns 1 when a digit was consumed, 0 when the
 * current character is not a digit. */
static int j89_int_digit(struct j89_parser *p, long *ival, int *iover, int neg)
{
    int c;
    int d;
    int digit;
    c = j89_cur(p);
    d = j89_is_digit(c);
    if (d == 0)
    {
        return 0;
    }
    digit = c - '0';
    if (*iover != 0)
    {
        j89_bump(p);
        return 1;
    }
    if (neg != 0)
    {
        j89_acc_neg(ival, iover, digit);
    }
    else
    {
        j89_acc_pos(ival, iover, digit);
    }
    j89_bump(p);
    return 1;
}

/* Consume all leading integer digits (the mantissa's integer part). */
static void j89_scan_int_digits(struct j89_parser *p, long *ival, int *iover,
                                int neg)
{
    int cont;
    cont = 1;
    while (cont != 0)
    {
        cont = j89_int_digit(p, ival, iover, neg);
    }
}

/* Consume the integer part of a number at the current position. Returns 1
 * when a valid integer part (a single '0' or a nonzero-leading digit run)
 * was present, 0 otherwise. Accumulates the value into *ival. */
static int j89_read_int_part(struct j89_parser *p, int neg, long *ival,
                             int *iover)
{
    int c;
    int d;
    c = j89_cur(p);
    if (c == '0')
    {
        j89_bump(p);
        return 1;
    }
    d = j89_is_digit(c);
    if (d != 0)
    {
        j89_scan_int_digits(p, ival, iover, neg);
        return 1;
    }
    return 0;
}

static int j89_frac_digit(struct j89_parser *p, int *frac)
{
    int c;
    int d;
    c = j89_cur(p);
    d = j89_is_digit(c);
    if (d == 0)
    {
        return 0;
    }
    *frac = *frac + 1;
    j89_bump(p);
    return 1;
}

static void j89_scan_frac_digits(struct j89_parser *p, int *frac)
{
    int cont;
    cont = 1;
    while (cont != 0)
    {
        cont = j89_frac_digit(p, frac);
    }
}

/* Consume an optional fractional part. Sets *is_float and the fraction digit
 * count. Returns 1 when there is no fraction or it is valid, 0 on a malformed
 * fraction ('." not followed by a digit). */
static int j89_take_frac(struct j89_parser *p, int *is_float, int *frac)
{
    int c;
    int d;
    c = j89_cur(p);
    if (c != '.')
    {
        return 1;
    }
    *is_float = 1;
    j89_bump(p);
    c = j89_cur(p);
    d = j89_is_digit(c);
    if (d == 0)
    {
        return 0;
    }
    j89_scan_frac_digits(p, frac);
    return 1;
}

static int j89_apply_exp(struct j89_parser *p, int *is_float, long *e)
{
    int r;
    *is_float = 1;
    r = j89_parse_exp(p, e);
    return r;
}

/* Consume an optional exponent ('e'/'E'...). Sets *is_float and *e. Returns
 * 1 when there is no exponent or it parsed, 0 when the exponent was invalid
 * (parse_exp already recorded the error). */
static int j89_take_exp(struct j89_parser *p, int *is_float, long *e)
{
    int c;
    int r;
    c = j89_cur(p);
    if (c != 'e')
    {
        if (c != 'E')
        {
            return 1;
        }
    }
    r = j89_apply_exp(p, is_float, e);
    return r;
}

/* Validate a freshly built scalar node, reporting out-of-memory when absent. */
static j89_len j89_finish_scalar(struct j89_parser *p, j89_len node)
{
    int bad;
    bad = j89_is_bad(node);
    if (bad)
    {
        j89_fail(p, "out of memory");
        return J89_BAD;
    }
    return node;
}

static double j89_apply_sign(double value, int neg)
{
    if (neg != 0)
    {
        return -value;
    }
    return value;
}

static j89_len j89_float_node(struct j89_parser *p, j89_arena *a, j89_len tok0,
                              j89_len sigend, long e, int frac, int neg)
{
    double value;
    j89_len node;
    int r;
    r = j89_token_to_double(p, tok0, sigend, e, frac, &value);
    if (r == -1)
    {
        j89_fail(p, "out of memory");
        return J89_BAD;
    }
    if (r == 1)
    {
        j89_fail(p, "number out of range");
        return J89_BAD;
    }
    value = j89_apply_sign(value, neg);
    node = j89_add_double(a, value);
    node = j89_finish_scalar(p, node);
    return node;
}

static j89_len j89_int_node(struct j89_parser *p, j89_arena *a, long ival)
{
    double dv;
    j89_len node;
    dv = (double)ival;
    node = j89_add_int(a, dv);
    node = j89_finish_scalar(p, node);
    return node;
}

static j89_len j89_parse_number(struct j89_parser *p)
{
    j89_arena *a;
    j89_len tok0;
    j89_len sigend;
    j89_len node;
    long ival;
    long e;
    int iover;
    int neg;
    int is_float;
    int frac;
    int okint;
    int fok;
    int rok;
    a = p->a;
    ival = 0;
    iover = 0;
    is_float = 0;
    e = 0;
    frac = 0;
    tok0 = p->pos;
    neg = j89_take_neg(p);
    okint = j89_read_int_part(p, neg, &ival, &iover);
    if (okint == 0)
    {
        j89_fail(p, "invalid number");
        return J89_BAD;
    }
    fok = j89_take_frac(p, &is_float, &frac);
    if (fok == 0)
    {
        j89_fail(p, "invalid number");
        return J89_BAD;
    }
    sigend = p->pos;
    rok = j89_take_exp(p, &is_float, &e);
    if (rok == 0)
    {
        return J89_BAD;
    }
    if (is_float)
    {
        node = j89_float_node(p, a, tok0, sigend, e, frac, neg);
        return node;
    }
    if (iover)
    {
        j89_fail(p, "integer out of range");
        return J89_BAD;
    }
    node = j89_int_node(p, a, ival);
    return node;
}

/* Consume one expected literal character at the current position. Returns 0
 * on a match (advanced), nonzero (with an error recorded) on a mismatch. */
static int j89_lit_char(struct j89_parser *p, int expect)
{
    int c;
    c = j89_cur(p);
    if (c != expect)
    {
        j89_fail(p, "invalid literal");
        return 1;
    }
    j89_bump(p);
    return 0;
}

static j89_len j89_parse_lit(struct j89_parser *p, const char *tok, j89_kind k)
{
    j89_arena *aa;
    j89_len node;
    j89_len i;
    int bad;
    aa = p->a;
    for (i = 0; tok[i] != '\0'; i = i + 1)
    {
        int st;
        st = j89_lit_char(p, tok[i]);
        if (st != 0)
        {
            return J89_BAD;
        }
    }
    node = j89_new_node(aa);
    bad = j89_is_bad(node);
    if (bad)
    {
        j89_fail(p, "out of memory");
        return J89_BAD;
    }
    j89_node_set_kind(aa, node, k);
    return node;
}

static j89_len j89_parse_value(struct j89_parser *p);
static j89_len j89_parse_array(struct j89_parser *p);
static j89_len j89_parse_object(struct j89_parser *p);

static j89_len j89_parse_nested(struct j89_parser *p, int c)
{
    j89_len node;
    j89_len cur;
    j89_len depth;
    j89_len dec;
    int over;
    cur = p->depth;
    depth = cur + 1;
    over = (depth > J89_MAX_DEPTH);
    if (over)
    {
        j89_fail(p, "maximum depth exceeded");
        return J89_BAD;
    }
    p->depth = depth;
    if (c == '{')
    {
        node = j89_parse_object(p);
    }
    else
    {
        node = j89_parse_array(p);
    }
    cur = p->depth;
    dec = cur - 1;
    p->depth = dec;
    return node;
}

/* Free *items and set it to NULL. */
static void j89_slots_clear(j89_len **items)
{
    free(*items);
    *items = NULL;
}

/* Report out of memory (freeing ptr first). Returns -1. */
static int j89_oom(struct j89_parser *p, void *ptr)
{
    if (ptr != NULL)
    {
        free(ptr);
    }
    j89_fail(p, "out of memory");
    return -1;
}
/* Next slot capacity, starting at 8 and doubling thereafter. */
static j89_len j89_next_cap(j89_len cur)
{
    if (cur == 0)
    {
        return 8;
    }
    return cur * 2;
}

/* Grow *items so another element slot is available. *cap is updated. On
 * allocation failure the old block is freed and an error recorded. Returns
 * 0 on success, -1 on allocation failure. */
static int j89_ary_reserve(struct j89_parser *p, j89_len **items, j89_len *cap)
{
    j89_len nc;
    j89_len sz;
    void *vp;
    j89_len *np;
    nc = j89_next_cap(*cap);
    sz = nc * sizeof(j89_len);
    vp = realloc(*items, sz);
    np = (j89_len *)vp;
    if (np == NULL)
    {
        int e;
        e = j89_oom(p, *items);
        return e;
    }
    *items = np;
    *cap = nc;
    return 0;
}

static void j89_consume_comma(struct j89_parser *p)
{
    j89_bump(p);
    j89_skip_ws(p);
}

/* Parse and append one array element. Returns 1 when a ',' was consumed (more
 * elements follow), 0 when the array ended (']' consumed), -1 on error (the
 * element buffer has been freed). */
static int j89_array_item(struct j89_parser *p, j89_len **childs, j89_len *cap,
                          j89_len *count)
{
    j89_len child;
    int ok;
    int c;
    if (*count == *cap)
    {
        int rs;
        rs = j89_ary_reserve(p, childs, cap);
        if (rs != 0)
        {
            return -1;
        }
    }
    child = j89_parse_value(p);
    ok = p->ok;
    if (ok == 0)
    {
        j89_slots_clear(childs);
        return -1;
    }
    (*childs)[*count] = child;
    *count = *count + 1;
    j89_skip_ws(p);
    c = j89_cur(p);
    if (c == ',')
    {
        j89_consume_comma(p);
        return 1;
    }
    if (c == ']')
    {
        j89_bump(p);
        return 0;
    }
    j89_fail(p, "expected , or ]");
    j89_slots_clear(childs);
    return -1;
}

static j89_len j89_array_empty(struct j89_parser *p, j89_arena *a)
{
    j89_len node;
    j89_bump(p);
    node = j89_make_array(a, 0);
    return node;
}

static j89_len j89_parse_array(struct j89_parser *p)
{
    j89_arena *a;
    j89_len *childs;
    j89_len cap;
    j89_len count;
    j89_len node;
    j89_len i;
    int c;
    int bad;
    a = p->a;
    j89_bump(p); /* consume '[' */
    cap = 0;
    count = 0;
    childs = NULL;
    j89_skip_ws(p);
    c = j89_cur(p);
    if (c == ']')
    {
        node = j89_array_empty(p, a);
        return node;
    }
    for (;;)
    {
        int st;
        st = j89_array_item(p, &childs, &cap, &count);
        if (st == 0)
        {
            break;
        }
        if (st == -1)
        {
            return J89_BAD;
        }
    }
    node = j89_make_array(a, count);
    bad = j89_is_bad(node);
    if (bad)
    {
        j89_oom(p, childs);
        return J89_BAD;
    }
    for (i = 0; i < count; i = i + 1)
    {
        j89_array_set_child(a, node, i, childs[i]);
    }
    free(childs);
    return node;
}

static void j89_members_clear(struct j89_pmember **items)
{
    free(*items);
    *items = NULL;
}

/* Grow *items (an array of struct j89_pmember) so another slot is available. */
static int j89_member_reserve(struct j89_parser *p, struct j89_pmember **items,
                              j89_len *cap)
{
    j89_len nc;
    j89_len sz;
    void *vp;
    struct j89_pmember *np;
    nc = j89_next_cap(*cap);
    sz = nc * sizeof(struct j89_pmember);
    vp = realloc(*items, sz);
    np = (struct j89_pmember *)vp;
    if (np == NULL)
    {
        int e;
        e = j89_oom(p, *items);
        return e;
    }
    *items = np;
    *cap = nc;
    return 0;
}

/* Free the member buffer and record msg. Returns -1. */
static int j89_member_err(struct j89_parser *p, struct j89_pmember **members,
                          const char *msg)
{
    j89_members_clear(members);
    j89_fail(p, msg);
    return -1;
}

/* True when ko/kl matches the key of any of the first count members. */
static int j89_has_dup(j89_arena *a, const struct j89_pmember *members,
                       j89_len count, j89_len ko, j89_len kl)
{
    j89_len j;
    for (j = 0; j < count; j = j + 1)
    {
        int eq;
        eq = j89_key_eq(a, ko, kl, members[j].ko, members[j].kl);
        if (eq != 0)
        {
            return 1;
        }
    }
    return 0;
}

/* Parse and append one object member. Returns 1 when a ',' was consumed (more
 * members follow), 0 when the object ended ('}' consumed), -1 on error (the
 * member buffer has been freed). */
static int j89_object_member(struct j89_parser *p, struct j89_pmember **members,
                             j89_len *cap, j89_len *count)
{
    j89_arena *a;
    j89_len key;
    j89_len ko;
    j89_len kl;
    j89_len value;
    int ok;
    int c;
    int dup;
    a = p->a;
    if (*count == *cap)
    {
        int rs;
        rs = j89_member_reserve(p, members, cap);
        if (rs != 0)
        {
            return -1;
        }
    }
    j89_skip_ws(p);
    c = j89_cur(p);
    if (c != '"')
    {
        int e2;
        e2 = j89_member_err(p, members, "expected object key");
        return e2;
    }
    key = j89_parse_string(p);
    ok = p->ok;
    if (ok == 0)
    {
        j89_members_clear(members);
        return -1;
    }
    ko = j89_node_base(a, key);
    kl = j89_node_n(a, key);
    j89_skip_ws(p);
    c = j89_cur(p);
    if (c != ':')
    {
        int e2;
        e2 = j89_member_err(p, members, "expected :");
        return e2;
    }
    j89_bump(p);
    j89_skip_ws(p);
    value = j89_parse_value(p);
    ok = p->ok;
    if (ok == 0)
    {
        j89_members_clear(members);
        return -1;
    }
    dup = j89_has_dup(a, *members, *count, ko, kl);
    if (dup != 0)
    {
        int e2;
        e2 = j89_member_err(p, members, "duplicate object key");
        return e2;
    }
    (*members)[*count].ko = ko;
    (*members)[*count].kl = kl;
    (*members)[*count].value = value;
    *count = *count + 1;
    j89_skip_ws(p);
    c = j89_cur(p);
    if (c == ',')
    {
        j89_bump(p);
        return 1;
    }
    if (c == '}')
    {
        j89_bump(p);
        return 0;
    }
    j89_fail(p, "expected , or }");
    j89_members_clear(members);
    return -1;
}

static j89_len j89_object_empty(struct j89_parser *p, j89_arena *a)
{
    j89_len node;
    j89_bump(p);
    node = j89_make_object(a, 0);
    return node;
}

static j89_len j89_parse_object(struct j89_parser *p)
{
    j89_arena *a;
    struct j89_pmember *members;
    j89_len cap;
    j89_len count;
    j89_len node;
    j89_len i;
    int c;
    int bad;
    a = p->a;
    j89_bump(p); /* consume '{' */
    cap = 0;
    count = 0;
    members = NULL;
    j89_skip_ws(p);
    c = j89_cur(p);
    if (c == '}')
    {
        node = j89_object_empty(p, a);
        return node;
    }
    for (;;)
    {
        int st;
        st = j89_object_member(p, &members, &cap, &count);
        if (st == 0)
        {
            break;
        }
        if (st == -1)
        {
            return J89_BAD;
        }
    }
    node = j89_make_object(a, count);
    bad = j89_is_bad(node);
    if (bad)
    {
        j89_oom(p, members);
        return J89_BAD;
    }
    for (i = 0; i < count; i = i + 1)
    {
        j89_object_set_member(a, node, i, members[i].ko, members[i].kl,
                              members[i].value);
    }
    free(members);
    return node;
}

static j89_len j89_parse_value(struct j89_parser *p)
{
    int c;
    int d;
    j89_len r;
    j89_skip_ws(p);
    c = j89_cur(p);
    if (c == '{')
    {
        r = j89_parse_nested(p, c);
        return r;
    }
    if (c == '[')
    {
        r = j89_parse_nested(p, c);
        return r;
    }
    if (c == '"')
    {
        r = j89_parse_string(p);
        return r;
    }
    if (c == 't')
    {
        r = j89_parse_lit(p, "true", J89_TRUE);
        return r;
    }
    if (c == 'f')
    {
        r = j89_parse_lit(p, "false", J89_FALSE);
        return r;
    }
    if (c == 'n')
    {
        r = j89_parse_lit(p, "null", J89_NULL);
        return r;
    }
    if (c == '-')
    {
        r = j89_parse_number(p);
        return r;
    }
    d = j89_is_digit(c);
    if (d)
    {
        r = j89_parse_number(p);
        return r;
    }
    j89_fail(p, "unexpected character");
    return J89_BAD;
}

/* Byte offset of a leading UTF-8 byte-order mark (EF BB BF), or 0. */
static j89_len j89_bom_start(const char *buf, j89_len len)
{
    unsigned char b0;
    unsigned char b1;
    unsigned char b2;
    if (len < 3)
    {
        return 0;
    }
    b0 = (unsigned char)buf[0];
    if (b0 != 0xEF)
    {
        return 0;
    }
    b1 = (unsigned char)buf[1];
    if (b1 != 0xBB)
    {
        return 0;
    }
    b2 = (unsigned char)buf[2];
    if (b2 != 0xBF)
    {
        return 0;
    }
    return 3;
}

j89_len j89_parse_bytes(const char *buf, j89_len len, j89_arena *a)
{
    struct j89_parser p;
    j89_len root;
    j89_len start;
    int e;
    int ok;
    start = j89_bom_start(buf, len);
    p.buf = buf;
    p.len = len;
    p.pos = start;
    p.a = a;
    p.ok = 1;
    p.depth = 0;
    root = j89_parse_value(&p);
    ok = p.ok;
    if (ok == 0)
    {
        return J89_BAD;
    }
    j89_skip_ws(&p);
    e = j89_eof(&p);
    if (e == 0)
    {
        j89_fail(&p, "trailing data");
        return J89_BAD;
    }
    return root;
}

j89_len j89_parse(const char *buf, j89_len len, j89_arena *a)
{
    j89_len r;
    r = j89_parse_bytes(buf, len, a);
    return r;
}
