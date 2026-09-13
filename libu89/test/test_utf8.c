#include <stdio.h>
#include <string.h>
#include "test.h"
#include "u89.h"

static const unsigned char B_OVERLONG_0[] = { 0xC0, 0x80 };          /* U+0000 */
static const unsigned char B_OVERLONG_2F[] = { 0xC0, 0xAF };         /* U+002F */
static const unsigned char B_SURROGATE[] = { 0xED, 0xA0, 0x80 };     /* U+D800 */
static const unsigned char B_ABOVE_10FFFF[] = { 0xF4, 0x90, 0x80, 0x80 };
static const unsigned char B_TRUNCATED[] = { 0xE2, 0x82 };           /* missing byte */
static const unsigned char B_LONE_CONT[] = { 0x80 };
static const unsigned char B_INVALID_LEAD[] = { 0xFE };
static const unsigned char B_SUPPLEMENTARY[] = { 0xF0, 0x9F, 0x8C, 0x80 }; /* U+1F300 */
static const unsigned char B_U0000[] = { 0x00 };
static const unsigned char B_U007F[] = { 0x7F };
static const unsigned char B_U0080[] = { 0xC2, 0x80 };
static const unsigned char B_U07FF[] = { 0xDF, 0xBF };
static const unsigned char B_U0800[] = { 0xE0, 0xA0, 0x80 };
static const unsigned char B_UD7FF[] = { 0xED, 0x9F, 0xBF };
static const unsigned char B_UE000[] = { 0xEE, 0x80, 0x80 };
static const unsigned char B_UFFFF[] = { 0xEF, 0xBF, 0xBF };
static const unsigned char B_U10000[] = { 0xF0, 0x90, 0x80, 0x80 };
static const unsigned char B_U10FFFF[] = { 0xF4, 0x8F, 0xBF, 0xBF };
static const unsigned char B_OVERLONG_3[] = { 0xE0, 0x80, 0x80 };
static const unsigned char B_OVERLONG_4[] = { 0xF0, 0x80, 0x80, 0x80 };
static const unsigned char B_ABOVE_2[] = { 0xF5, 0x80, 0x80, 0x80 };
static const unsigned char B_LONE_CONT_BF[] = { 0xBF };
static const unsigned char B_INVALID_C1[] = { 0xC1, 0xBF };
static const unsigned char B_INVALID_FF[] = { 0xFF };
static const unsigned char B_TRUNC_2[] = { 0xC2 };
static const unsigned char B_TRUNC_4[] = { 0xF0, 0x9F, 0x8C };
static const unsigned char B_BAD_CONT_1[] = { 0xC2, 0x41 };
static const unsigned char B_BAD_CONT_2[] = { 0xE2, 0x82, 0x41 };
static const unsigned char B_BAD_CONT_3[] = { 0xF0, 0x9F, 0x8C, 0x41 };
static const unsigned char B_MIXED[] = {
    'a', 0xC3, 0xA9, 0xE2, 0x82, 0xAC, 0xF0, 0x9F, 0x98, 0x80, 0xCC, 0x81
};
static const unsigned char B_NUL[] = { 0x61, 0x00, 0x62 };

static void decode_case(const unsigned char *s, size_t n, size_t pos,
                        u89_status want, u89_cp want_cp, size_t want_next,
                        const char *what)
{
    char ctx[96];
    u89_cp cp;
    size_t next;
    u89_status st;

    sprintf(ctx, "decode: %s", what);
    cp = 0;
    next = 0;
    st = u89_utf8_decode(s, n, pos, &cp, &next);
    u89_check_ctx(st == want, "decode status", ctx);
    if (st == U89_OK) {
        u89_check_ctx(cp == want_cp, "decode scalar", ctx);
        u89_check_ctx(next == want_next, "decode next", ctx);
    }
}

static void prev_case(const unsigned char *s, size_t n, size_t pos,
                      u89_status want, u89_cp want_cp, size_t want_prev,
                      const char *what)
{
    char ctx[96];
    u89_cp cp;
    size_t prev;
    u89_status st;

    sprintf(ctx, "prev: %s", what);
    cp = 0;
    prev = 0;
    st = u89_utf8_prev(s, n, pos, &cp, &prev);
    u89_check_ctx(st == want, "prev status", ctx);
    if (st == U89_OK) {
        u89_check_ctx(cp == want_cp, "prev scalar", ctx);
        u89_check_ctx(prev == want_prev, "prev offset", ctx);
    }
}

static void reject(const unsigned char *s, size_t n, const char *what)
{
    char ctx[80];
    sprintf(ctx, "reject: %s", what);
    u89_check_ctx(u89_utf8_valid(s, n) == 0, "invalid utf8 must be rejected", ctx);
}

static void accept(const unsigned char *s, size_t n, const char *what)
{
    char ctx[80];
    sprintf(ctx, "accept: %s", what);
    u89_check_ctx(u89_utf8_valid(s, n) == 1, "valid utf8 must be accepted", ctx);
}

void test_utf8(void)
{
    /* A. overlong */
    reject(B_OVERLONG_0, sizeof B_OVERLONG_0, "overlong U+0000");
    reject(B_OVERLONG_2F, sizeof B_OVERLONG_2F, "overlong U+002F");
    /* B. surrogate */
    reject(B_SURROGATE, sizeof B_SURROGATE, "utf8 surrogate");
    /* C. above U+10FFFF */
    reject(B_ABOVE_10FFFF, sizeof B_ABOVE_10FFFF, "above 10FFFF");
    /* D. truncated */
    reject(B_TRUNCATED, sizeof B_TRUNCATED, "truncated");
    /* E. lone continuation + invalid lead */
    reject(B_LONE_CONT, sizeof B_LONE_CONT, "lone continuation");
    reject(B_INVALID_LEAD, sizeof B_INVALID_LEAD, "invalid lead");

    /* F. scalar boundary: surrogates are not scalars */
    u89_check(u89_is_scalar(0xD800UL) == 0, "0xD800 is not a scalar");
    u89_check(u89_is_scalar(0xDFFFUL) == 0, "0xDFFF is not a scalar");
    u89_check(u89_is_scalar(0x0000UL) == 1, "0x0000 is a scalar");
    u89_check(u89_is_scalar(0xD7FFUL) == 1, "0xD7FF is a scalar");
    u89_check(u89_is_scalar(0xE000UL) == 1, "0xE000 is a scalar");
    u89_check(u89_is_scalar(0x10FFFFUL) == 1, "0x10FFFF is a scalar");
    u89_check(u89_is_scalar(0x110000UL) == 0, "0x110000 is not a scalar");

    /* G/H. iterate: valid supplementary + embedded NUL round-trip */
    {
        unsigned char b[] = { 'a', 0x00, 0xF0, 0x9F, 0x8C, 0x80, 0xE2, 0x82, 0xAC };
        u89_iter it;
        u89_cp got[4];
        size_t n = 0;
        u89_iter_init(&it, b, sizeof b);
        while (u89_iter_next(&it) == 0 && n < 4) {
            got[n] = it.cp;
            n++;
        }
        u89_check(n == 4, "iter decodes 4 scalars");
        u89_check(it.err == 2, "iter ends cleanly at end");
        u89_check(n == 0 || got[0] == 0x61UL, "iter first 'a'");
        u89_check(n >= 2 && got[1] == 0x0000UL, "iter preserves NUL");
        u89_check(n >= 3 && got[2] == 0x1F300UL, "iter supplementary");
        u89_check(n >= 4 && got[3] == 0x20ACUL, "iter euro");
    }

    /* H. iterator rejects malformed, does not recover to U+FFFD */
    {
        unsigned char b[] = { 'a', 0xC0, 0x80 };
        u89_iter it;
        u89_iter_init(&it, b, sizeof b);
        u89_check(u89_iter_next(&it) == 0, "iter first ok");
        u89_check(u89_iter_next(&it) != 0, "iter stops on malformed");
        u89_check(it.err == 1, "iter err==1 on malformed");
        u89_check(it.cp != 0xFFFDuL, "iter never emits U+FFFD");
    }

    /* G. accept valid + supplementary */
    accept(B_SUPPLEMENTARY, sizeof B_SUPPLEMENTARY, "supplementary");
    accept((const unsigned char *)"hello", 5, "ascii");

    /* u89_utf8_len / encode round-trip */
    {
        unsigned char out[4];
        int len = u89_utf8_len(0x1F300UL);
        u89_check(len == 4, "utf8_len supplementary == 4");
        u89_check(u89_utf8_encode(0x1F300UL, out) == 4, "encode supplementary");
        u89_check(memcmp(out, B_SUPPLEMENTARY, 4) == 0, "encode bytes match");
        u89_check(u89_utf8_len(0x0041UL) == 1, "utf8_len ascii == 1");
        u89_check(u89_utf8_len(0xD800UL) == 0, "utf8_len surrogate == 0");
    }

    /* U8-01..04: positional decode at scalar boundaries */
    decode_case(B_U0000, sizeof B_U0000, 0, U89_OK, 0x0000UL, 1, "U+0000");
    decode_case(B_U007F, sizeof B_U007F, 0, U89_OK, 0x007FUL, 1, "U+007F");
    decode_case(B_U0080, sizeof B_U0080, 0, U89_OK, 0x0080UL, 2, "U+0080");
    decode_case(B_U07FF, sizeof B_U07FF, 0, U89_OK, 0x07FFUL, 2, "U+07FF");
    decode_case(B_U0800, sizeof B_U0800, 0, U89_OK, 0x0800UL, 3, "U+0800");
    decode_case(B_UD7FF, sizeof B_UD7FF, 0, U89_OK, 0xD7FFUL, 3, "U+D7FF");
    decode_case(B_UE000, sizeof B_UE000, 0, U89_OK, 0xE000UL, 3, "U+E000");
    decode_case(B_UFFFF, sizeof B_UFFFF, 0, U89_OK, 0xFFFFUL, 3, "U+FFFF");
    decode_case(B_U10000, sizeof B_U10000, 0, U89_OK, 0x10000UL, 4, "U+10000");
    decode_case(B_U10FFFF, sizeof B_U10FFFF, 0, U89_OK, 0x10FFFFUL, 4, "U+10FFFF");

    /* U8-05..10: positional decode rejects malformed input */
    decode_case(B_OVERLONG_0, sizeof B_OVERLONG_0, 0, U89_EUTF8, 0, 0, "overlong 2");
    decode_case(B_OVERLONG_3, sizeof B_OVERLONG_3, 0, U89_EUTF8, 0, 0, "overlong 3");
    decode_case(B_OVERLONG_4, sizeof B_OVERLONG_4, 0, U89_EUTF8, 0, 0, "overlong 4");
    decode_case(B_SURROGATE, sizeof B_SURROGATE, 0, U89_EUTF8, 0, 0, "surrogate");
    decode_case(B_ABOVE_10FFFF, sizeof B_ABOVE_10FFFF, 0, U89_EUTF8, 0, 0,
                "above 10FFFF");
    decode_case(B_ABOVE_2, sizeof B_ABOVE_2, 0, U89_EUTF8, 0, 0, "above 10FFFF 2");
    decode_case(B_LONE_CONT, sizeof B_LONE_CONT, 0, U89_EUTF8, 0, 0, "lone 80");
    decode_case(B_LONE_CONT_BF, sizeof B_LONE_CONT_BF, 0, U89_EUTF8, 0, 0, "lone BF");
    decode_case(B_INVALID_LEAD, sizeof B_INVALID_LEAD, 0, U89_EUTF8, 0, 0, "FE lead");
    decode_case(B_INVALID_FF, sizeof B_INVALID_FF, 0, U89_EUTF8, 0, 0, "FF lead");
    decode_case(B_INVALID_C1, sizeof B_INVALID_C1, 0, U89_EUTF8, 0, 0, "C1 lead");
    decode_case(B_TRUNC_2, sizeof B_TRUNC_2, 0, U89_EUTF8, 0, 0, "truncated 2");
    decode_case(B_TRUNCATED, sizeof B_TRUNCATED, 0, U89_EUTF8, 0, 0, "truncated 3");
    decode_case(B_TRUNC_4, sizeof B_TRUNC_4, 0, U89_EUTF8, 0, 0, "truncated 4");
    decode_case(B_BAD_CONT_1, sizeof B_BAD_CONT_1, 0, U89_EUTF8, 0, 0, "bad cont 2");
    decode_case(B_BAD_CONT_2, sizeof B_BAD_CONT_2, 0, U89_EUTF8, 0, 0, "bad cont 3");
    decode_case(B_BAD_CONT_3, sizeof B_BAD_CONT_3, 0, U89_EUTF8, 0, 0, "bad cont 4");

    /* U8-12: position at or past the end */
    decode_case(B_MIXED, sizeof B_MIXED, sizeof B_MIXED, U89_ERANGE, 0, 0, "pos == n");
    decode_case(B_MIXED, sizeof B_MIXED, sizeof B_MIXED + 1, U89_ERANGE, 0, 0,
                "pos > n");

    /* U8-16: interior positions */
    decode_case(B_MIXED, sizeof B_MIXED, 1, U89_OK, 0x00E9UL, 3, "interior e-acute");
    decode_case(B_MIXED, sizeof B_MIXED, 3, U89_OK, 0x20ACUL, 6, "interior euro");
    decode_case(B_MIXED, sizeof B_MIXED, 6, U89_OK, 0x1F600UL, 10, "interior emoji");

    /* U8-13/14: mixed string and embedded NUL validate */
    u89_check(u89_utf8_valid(B_MIXED, sizeof B_MIXED) == 1, "mixed valid");
    u89_check(u89_utf8_valid(B_NUL, sizeof B_NUL) == 1, "NUL valid");
    decode_case(B_NUL, sizeof B_NUL, 1, U89_OK, 0x0000UL, 2, "embedded NUL");

    /* U8-11: forward/backward agreement across the mixed string */
    {
        size_t p = 0;
        while (p < sizeof B_MIXED) {
            u89_cp fwd = 0;
            u89_cp back = 0;
            size_t next = 0;
            size_t start = 0;
            u89_check(u89_utf8_decode(B_MIXED, sizeof B_MIXED, p, &fwd, &next)
                          == U89_OK,
                      "fwd decode in walk");
            u89_check(u89_utf8_prev(B_MIXED, sizeof B_MIXED, next, &back, &start)
                          == U89_OK,
                      "back decode in walk");
            u89_check(fwd == back && start == p, "forward/backward agree");
            p = next;
        }
    }

    /* U8-15/17: prev range, malformed, and mid-scalar */
    prev_case(B_MIXED, sizeof B_MIXED, 0, U89_ERANGE, 0, 0, "pos 0");
    prev_case(B_MIXED, sizeof B_MIXED, sizeof B_MIXED + 1, U89_ERANGE, 0, 0,
              "pos > n");
    prev_case(B_OVERLONG_0, sizeof B_OVERLONG_0, 2, U89_EUTF8, 0, 0,
              "malformed before");
    prev_case(B_MIXED, sizeof B_MIXED, 2, U89_EUTF8, 0, 0, "mid-scalar");
    prev_case(B_MIXED, sizeof B_MIXED, 3, U89_OK, 0x00E9UL, 1, "e-acute backwards");
    prev_case(B_MIXED, sizeof B_MIXED, sizeof B_MIXED, U89_OK, 0x0301UL, 10,
              "final mark");

    /* U8-19: expected sequence length from a lead byte */
    u89_check(u89_utf8_seq_len(0x00) == 1, "seq_len NUL");
    u89_check(u89_utf8_seq_len(0x7F) == 1, "seq_len 7F");
    u89_check(u89_utf8_seq_len(0xC2) == 2, "seq_len C2");
    u89_check(u89_utf8_seq_len(0xDF) == 2, "seq_len DF");
    u89_check(u89_utf8_seq_len(0xE0) == 3, "seq_len E0");
    u89_check(u89_utf8_seq_len(0xEF) == 3, "seq_len EF");
    u89_check(u89_utf8_seq_len(0xF0) == 4, "seq_len F0");
    u89_check(u89_utf8_seq_len(0xF4) == 4, "seq_len F4");
    u89_check(u89_utf8_seq_len(0x80) == 0, "seq_len continuation 80");
    u89_check(u89_utf8_seq_len(0xBF) == 0, "seq_len continuation BF");
    u89_check(u89_utf8_seq_len(0xC0) == 0, "seq_len C0");
    u89_check(u89_utf8_seq_len(0xC1) == 0, "seq_len C1");
    u89_check(u89_utf8_seq_len(0xF5) == 0, "seq_len F5");
    u89_check(u89_utf8_seq_len(0xFF) == 0, "seq_len FF");

    /* utf16 units */
    u89_check(u89_utf16_units(0x0041UL) == 1, "bmp = 1 unit");
    u89_check(u89_utf16_units(0x1F300UL) == 2, "supplementary = 2 units");
    u89_check(u89_utf16_units(0xD800UL) == 0, "non-scalar = 0 units");

    /* UTF-16 high-surrogate classification boundaries */
    u89_check(u89_utf16_is_high_surrogate(0xD7FFU) == 0, "D7FF not high");
    u89_check(u89_utf16_is_high_surrogate(0xD800U) == 1, "D800 high");
    u89_check(u89_utf16_is_high_surrogate(0xDBFFU) == 1, "DBFF high");
    u89_check(u89_utf16_is_high_surrogate(0xDC00U) == 0, "DC00 not high");
    u89_check(u89_utf16_is_high_surrogate(0xDFFFU) == 0, "DFFF not high");
    u89_check(u89_utf16_is_high_surrogate(0xE000U) == 0, "E000 not high");
    u89_check(u89_utf16_is_high_surrogate(0x10000U) == 0, "above FFFF not high");

    /* UTF-16 low-surrogate classification boundaries */
    u89_check(u89_utf16_is_low_surrogate(0xD7FFU) == 0, "D7FF not low");
    u89_check(u89_utf16_is_low_surrogate(0xD800U) == 0, "D800 not low");
    u89_check(u89_utf16_is_low_surrogate(0xDBFFU) == 0, "DBFF not low");
    u89_check(u89_utf16_is_low_surrogate(0xDC00U) == 1, "DC00 low");
    u89_check(u89_utf16_is_low_surrogate(0xDFFFU) == 1, "DFFF low");
    u89_check(u89_utf16_is_low_surrogate(0xE000U) == 0, "E000 not low");
    u89_check(u89_utf16_is_low_surrogate(0x10000U) == 0, "above FFFF not low");

    /* valid surrogate pairs decode to their supplementary scalar */
    {
        u89_cp cp;
        int ok;

        cp = 0;
        ok = u89_utf16_decode_pair(0xD800U, 0xDC00U, &cp);
        u89_check(ok == 1, "D800 DC00 decodes");
        u89_check(cp == 0x10000UL, "D800 DC00 -> U+10000");

        cp = 0;
        ok = u89_utf16_decode_pair(0xD800U, 0xDFFFU, &cp);
        u89_check(ok == 1, "D800 DFFF decodes");
        u89_check(cp == 0x103FFUL, "D800 DFFF -> U+103FF");

        cp = 0;
        ok = u89_utf16_decode_pair(0xDBFFU, 0xDC00U, &cp);
        u89_check(ok == 1, "DBFF DC00 decodes");
        u89_check(cp == 0x10FC00UL, "DBFF DC00 -> U+10FC00");

        cp = 0;
        ok = u89_utf16_decode_pair(0xDBFFU, 0xDFFFU, &cp);
        u89_check(ok == 1, "DBFF DFFF decodes");
        u89_check(cp == 0x10FFFFUL, "DBFF DFFF -> U+10FFFF");
    }

    /* invalid pairs fail and leave *cp unchanged */
    {
        u89_cp cp;
        int ok;

        cp = 0x42UL;
        ok = u89_utf16_decode_pair(0xD800U, 0x0041U, &cp);
        u89_check(ok == 0, "high + BMP fails");
        u89_check(cp == 0x42UL, "cp unchanged after high + BMP");

        cp = 0x42UL;
        ok = u89_utf16_decode_pair(0x0041U, 0xDC00U, &cp);
        u89_check(ok == 0, "BMP + low fails");
        u89_check(cp == 0x42UL, "cp unchanged after BMP + low");

        cp = 0x42UL;
        ok = u89_utf16_decode_pair(0xD800U, 0xD800U, &cp);
        u89_check(ok == 0, "high + high fails");
        u89_check(cp == 0x42UL, "cp unchanged after high + high");

        cp = 0x42UL;
        ok = u89_utf16_decode_pair(0xDC00U, 0xDC00U, &cp);
        u89_check(ok == 0, "low + low fails");
        u89_check(cp == 0x42UL, "cp unchanged after low + low");

        cp = 0x42UL;
        ok = u89_utf16_decode_pair(0xD7FFU, 0xDC00U, &cp);
        u89_check(ok == 0, "D7FF is not a high surrogate");
        u89_check(cp == 0x42UL, "cp unchanged after D7FF pair");
    }

    /* cp == NULL performs only the validity test */
    u89_check(u89_utf16_decode_pair(0xD800U, 0xDC00U, NULL) == 1,
              "valid pair with cp == NULL");
    u89_check(u89_utf16_decode_pair(0xD800U, 0x0041U, NULL) == 0,
              "invalid pair with cp == NULL");
}
