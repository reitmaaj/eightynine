/* u89_utf8.c - libu89 scalar core: UTF-8 validation, iteration, encoding. */

#include "../include/u89.h"

#define U89_MAX (0x10FFFFUL)
#define U89_BAD ((size_t)-1)

static int between(u89_cp cp, u89_cp lo, u89_cp hi)
{
    if (cp < lo)
    {
        return 0;
    }
    if (cp > hi)
    {
        return 0;
    }
    return 1;
}

int u89_is_scalar(u89_cp cp)
{
    int a;
    int b;

    a = between(cp, 0x0000UL, 0xD7FFUL);
    if (a)
    {
        return 1;
    }
    b = between(cp, 0xE000UL, U89_MAX);
    if (b)
    {
        return 1;
    }
    return 0;
}

/* Write cp to *out when out is not null. */
static void emit_out(u89_cp *out, u89_cp cp)
{
    if (out != NULL)
    {
        *out = cp;
    }
}

/* Decode a 2-byte sequence at s (lead already known valid C2..DF). Returns
   2 on success, 0 on malformed/truncated input. */
static int decode_two(const unsigned char *s, size_t n, u89_cp *out)
{
    unsigned char b;
    unsigned char c1;
    u89_cp cp;

    if (n < 2)
    {
        return 0;
    }
    c1 = s[1];
    if ((c1 & 0xC0) != 0x80)
    {
        return 0;
    }
    b = s[0];
    cp = (((u89_cp)b & 0x1F) << 6) | ((u89_cp)c1 & 0x3F);
    emit_out(out, cp);
    return 2;
}

/* Decode a 3-byte sequence at s (lead E0..EF), rejecting overlong and
   surrogate encodings. Returns 3 on success, 0 otherwise. */
static int decode_three(const unsigned char *s, size_t n, u89_cp *out)
{
    unsigned char b;
    unsigned char c1;
    unsigned char c2;
    u89_cp cp;

    if (n < 3)
    {
        return 0;
    }
    c1 = s[1];
    b = s[0];
    if (b == 0xE0)
    {
        if (c1 < 0xA0)
        {
            return 0;
        }
    }
    if (b == 0xED)
    {
        if (c1 > 0x9F)
        {
            return 0;
        }
    }
    if ((c1 & 0xC0) != 0x80)
    {
        return 0;
    }
    c2 = s[2];
    if ((c2 & 0xC0) != 0x80)
    {
        return 0;
    }
    cp = (((u89_cp)b & 0x0F) << 12) | (((u89_cp)c1 & 0x3F) << 6) |
         ((u89_cp)c2 & 0x3F);
    emit_out(out, cp);
    return 3;
}

/* Decode a 4-byte sequence at s (lead F0..F4), rejecting overlong encodings
   and values above U+10FFFF. Returns 4 on success, 0 otherwise. */
static int decode_four(const unsigned char *s, size_t n, u89_cp *out)
{
    unsigned char b;
    unsigned char c1;
    unsigned char c2;
    unsigned char c3;
    u89_cp cp;

    if (n < 4)
    {
        return 0;
    }
    c1 = s[1];
    b = s[0];
    if (b == 0xF0)
    {
        if (c1 < 0x90)
        {
            return 0;
        }
    }
    if (b == 0xF4)
    {
        if (c1 > 0x8F)
        {
            return 0;
        }
    }
    if ((c1 & 0xC0) != 0x80)
    {
        return 0;
    }
    c2 = s[2];
    if ((c2 & 0xC0) != 0x80)
    {
        return 0;
    }
    c3 = s[3];
    if ((c3 & 0xC0) != 0x80)
    {
        return 0;
    }
    cp = (((u89_cp)b & 0x07) << 18) | (((u89_cp)c1 & 0x3F) << 12) |
         (((u89_cp)c2 & 0x3F) << 6) | ((u89_cp)c3 & 0x3F);
    emit_out(out, cp);
    return 4;
}

/* Decode one UTF-8 sequence at s with n bytes available. Returns bytes
   consumed (1..4), or 0 if malformed or truncated. Writes the scalar to *out
   only on success. */
static int decode_one(const unsigned char *s, size_t n, u89_cp *out)
{
    unsigned char b;
    int r;
    int res;

    if (n == 0)
    {
        return 0;
    }
    b = s[0];
    if (b < 0x80)
    {
        emit_out(out, (u89_cp)b);
        return 1;
    }
    r = between(b, 0xC2, 0xDF);
    if (r)
    {
        res = decode_two(s, n, out);
        return res;
    }
    r = between(b, 0xE0, 0xEF);
    if (r)
    {
        res = decode_three(s, n, out);
        return res;
    }
    r = between(b, 0xF0, 0xF4);
    if (r)
    {
        res = decode_four(s, n, out);
        return res;
    }
    return 0;
}

u89_status u89_utf8_decode(const unsigned char *s, size_t n, size_t pos,
                           u89_cp *cp, size_t *next)
{
    int d;
    size_t adv;

    if (pos >= n)
    {
        return U89_ERANGE;
    }
    d = decode_one(s + pos, n - pos, cp);
    if (d == 0)
    {
        return U89_EUTF8;
    }
    adv = pos + (size_t)d;
    if (next != NULL)
    {
        *next = adv;
    }
    return U89_OK;
}

/* Lower bound for the backward scan: pos-4, or 0. */
static size_t prev_floor(size_t pos)
{
    if (pos > 4)
    {
        return pos - 4;
    }
    return 0;
}

/* Move *i one continuation byte toward the lead while allowed. Returns 1
   when *i moved, 0 when the scan must stop. */
static int prev_step(const unsigned char *s, size_t *i, size_t floor)
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

/* Scan back at most three continuation bytes to the lead of the scalar ending
   at pos. Returns the lead offset, or U89_BAD when the bytes cannot form a
   scalar ending exactly at pos. */
static size_t prev_lead(const unsigned char *s, size_t pos, u89_cp *out)
{
    size_t i;
    size_t floor;
    int d;
    int go;

    i = pos - 1;
    floor = prev_floor(pos);
    go = 1;
    while (go != 0)
    {
        go = prev_step(s, &i, floor);
    }
    d = decode_one(s + i, pos - i, out);
    if (d == 0)
    {
        return U89_BAD;
    }
    if (i + (size_t)d != pos)
    {
        return U89_BAD;
    }
    return i;
}

u89_status u89_utf8_prev(const unsigned char *s, size_t n, size_t pos,
                         u89_cp *cp, size_t *prev)
{
    size_t lead;
    u89_cp out;

    if (pos == 0)
    {
        return U89_ERANGE;
    }
    if (pos > n)
    {
        return U89_ERANGE;
    }
    out = 0;
    lead = prev_lead(s, pos, &out);
    if (lead == U89_BAD)
    {
        return U89_EUTF8;
    }
    if (cp != NULL)
    {
        *cp = out;
    }
    if (prev != NULL)
    {
        *prev = lead;
    }
    return U89_OK;
}

/* Advance index i past one valid scalar. Returns the new index, or U89_BAD
   when the bytes at i are malformed. */
static size_t decode_adv(const unsigned char *s, size_t n, size_t i)
{
    int d;
    u89_cp cp;

    d = decode_one(s + i, n - i, &cp);
    if (d == 0)
    {
        return U89_BAD;
    }
    i = i + (size_t)d;
    return i;
}

int u89_utf8_valid(const unsigned char *s, size_t n)
{
    size_t i;

    i = 0;
    while (i < n)
    {
        i = decode_adv(s, n, i);
        if (i == U89_BAD)
        {
            return 0;
        }
    }
    return 1;
}

int u89_utf8_seq_len(unsigned char lead)
{
    int r;

    if (lead < 0x80)
    {
        return 1;
    }
    r = between((u89_cp)lead, 0xC2, 0xDF);
    if (r)
    {
        return 2;
    }
    r = between((u89_cp)lead, 0xE0, 0xEF);
    if (r)
    {
        return 3;
    }
    r = between((u89_cp)lead, 0xF0, 0xF4);
    if (r)
    {
        return 4;
    }
    return 0;
}

int u89_utf8_len(u89_cp cp)
{
    int sc;

    sc = u89_is_scalar(cp);
    if (!sc)
    {
        return 0;
    }
    if (cp < 0x80UL)
    {
        return 1;
    }
    if (cp < 0x800UL)
    {
        return 2;
    }
    if (cp < 0x10000UL)
    {
        return 3;
    }
    return 4;
}

static void put_byte(unsigned char *dst, size_t i, unsigned int v)
{
    unsigned char uc;

    uc = (unsigned char)v;
    dst[i] = uc;
}

static int encode_one(u89_cp cp, unsigned char *out)
{
    put_byte(out, 0, cp);
    return 1;
}

static int encode_two(u89_cp cp, unsigned char *out)
{
    put_byte(out, 0, 0xC0 | (cp >> 6));
    put_byte(out, 1, 0x80 | (cp & 0x3F));
    return 2;
}

static int encode_three(u89_cp cp, unsigned char *out)
{
    put_byte(out, 0, 0xE0 | (cp >> 12));
    put_byte(out, 1, 0x80 | ((cp >> 6) & 0x3F));
    put_byte(out, 2, 0x80 | (cp & 0x3F));
    return 3;
}

static int encode_four(u89_cp cp, unsigned char *out)
{
    put_byte(out, 0, 0xF0 | (cp >> 18));
    put_byte(out, 1, 0x80 | ((cp >> 12) & 0x3F));
    put_byte(out, 2, 0x80 | ((cp >> 6) & 0x3F));
    put_byte(out, 3, 0x80 | (cp & 0x3F));
    return 4;
}

int u89_utf8_encode(u89_cp cp, unsigned char *out)
{
    int sc;
    int r;

    sc = u89_is_scalar(cp);
    if (!sc)
    {
        return 0;
    }
    if (cp < 0x80UL)
    {
        r = encode_one(cp, out);
        return r;
    }
    if (cp < 0x800UL)
    {
        r = encode_two(cp, out);
        return r;
    }
    if (cp < 0x10000UL)
    {
        r = encode_three(cp, out);
        return r;
    }
    r = encode_four(cp, out);
    return r;
}

void u89_iter_init(u89_iter *it, const unsigned char *s, size_t n)
{
    const unsigned char *e;

    it->cur = s;
    e = s + n;
    it->end = e;
    it->cp = 0;
    it->err = 0;
    it->leading = 0;
}

int u89_iter_next(u89_iter *it)
{
    const unsigned char *cur;
    const unsigned char *end;
    const unsigned char *np;
    size_t rem;
    int d;
    u89_cp cp;

    if (it->err != 0)
    {
        return 1;
    }
    cur = it->cur;
    end = it->end;
    if (cur >= end)
    {
        it->err = 2;
        return 1;
    }
    rem = (size_t)(end - cur);
    d = decode_one(cur, rem, &cp);
    if (d == 0)
    {
        it->err = 1;
        return 1;
    }
    it->cp = cp;
    it->leading = d;
    np = cur + d;
    it->cur = np;
    return 0;
}

int u89_utf16_units(u89_cp cp)
{
    int sc;

    sc = u89_is_scalar(cp);
    if (!sc)
    {
        return 0;
    }
    if (cp < 0x10000UL)
    {
        return 1;
    }
    return 2;
}

int u89_utf16_is_high_surrogate(unsigned int unit)
{
    int r;

    r = between(unit, 0xD800U, 0xDBFFU);
    return r;
}

int u89_utf16_is_low_surrogate(unsigned int unit)
{
    int r;

    r = between(unit, 0xDC00U, 0xDFFFU);
    return r;
}

int u89_utf16_decode_pair(unsigned int high, unsigned int low, u89_cp *cp)
{
    int h;
    int l;
    u89_cp v;

    h = u89_utf16_is_high_surrogate(high);
    l = u89_utf16_is_low_surrogate(low);
    if (h == 0)
    {
        return 0;
    }
    if (l == 0)
    {
        return 0;
    }
    v = ((high - 0xD800U) << 10) + (low - 0xDC00U);
    v = v + 0x10000U;
    if (cp != NULL)
    {
        *cp = v;
    }
    return 1;
}

/* White_Space property (UAX #44), private subset plus explicit singletons. */
static int ws_range(u89_cp cp)
{
    int a;
    int b;

    a = between(cp, 0x09, 0x0D);
    if (a)
    {
        return 1;
    }
    b = between(cp, 0x2000, 0x200A);
    if (b)
    {
        return 1;
    }
    return 0;
}

int u89_is_whitespace(u89_cp cp)
{
    int r;

    r = ws_range(cp);
    if (r)
    {
        return 1;
    }
    if (cp == 0x20)
    {
        return 1;
    }
    if (cp == 0x85)
    {
        return 1;
    }
    if (cp == 0xA0)
    {
        return 1;
    }
    if (cp == 0x1680)
    {
        return 1;
    }
    if (cp == 0x2028)
    {
        return 1;
    }
    if (cp == 0x2029)
    {
        return 1;
    }
    if (cp == 0x202F)
    {
        return 1;
    }
    if (cp == 0x205F)
    {
        return 1;
    }
    if (cp == 0x3000)
    {
        return 1;
    }
    return 0;
}
