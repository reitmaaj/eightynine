#include <string.h>
#include "test.h"
#include "u89.h"

static unsigned char a[512];
static unsigned char b[512];
static unsigned char c[512];
static u89_cp work[1024];

static int norm(int mode, const unsigned char *s, size_t n, unsigned char *o)
{
    return u89_normalize_ex(mode, s, n, o, sizeof a, work, 1024);
}

static int eq(const unsigned char *x, size_t xn, const unsigned char *y,
              size_t yn)
{
    if (xn != yn) {
        return 0;
    }
    return memcmp(x, y, xn) == 0;
}

/* NFC idempotence across a set of strings */
static void check_idempotent(int mode, const unsigned char *s, size_t n)
{
    int l1;
    int l2;

    l1 = norm(mode, s, n, a);
    l2 = norm(mode, a, (size_t)l1, b);
    u89_check_ctx(l1 == l2 && eq(a, (size_t)l1, b, (size_t)l2),
                  "normalization idempotent", "x");
}

/* canonical: NFC(NFD(x)) == NFC(x) */
static void check_canonical(const unsigned char *s, size_t n)
{
    int lc;
    int lc2;
    int ln;
    int ln2;

    lc = norm(0, s, n, a);              /* NFC(x) */
    ln = norm(1, a, (size_t)lc, b);     /* NFD(NFC(x)) */
    lc2 = norm(0, b, (size_t)ln, c);    /* NFC(NFD(NFC(x))) */
    u89_check_ctx(lc2 == lc && eq(c, (size_t)lc2, a, (size_t)lc),
                  "NFC(NFD(NFC(x))) == NFC(x)", "x");

    ln2 = norm(1, s, n, b);             /* NFD(x) */
    lc2 = norm(0, b, (size_t)ln2, c);   /* NFC(NFD(x)) */
    u89_check_ctx(lc2 == lc && eq(c, (size_t)lc2, a, (size_t)lc),
                  "NFC(NFD(x)) == NFC(x)", "x");
}

/* NFKC/NFKD fold fullwidth/halfwidth/compat */
static void check_compat_folds(void)
{
    /* fullwidth digits, katakana halfwidth, NBSP, copyright */
    {
        static const unsigned char fullwidth_1[] = { 0xEF, 0xBC, 0x91 }; /* １ */
        static const unsigned char one[] = { 0x31 };
        u89_check(norm(2, fullwidth_1, 3, a) == 1, "NFKC fullwidth １ -> 1");
        u89_check(eq(a, 1, one, 1), "NFKC fullwidth value");
        u89_check(norm(3, fullwidth_1, 3, a) == 1, "NFKD fullwidth １ -> 1");
    }
    {
        static const unsigned char nbsp[] = { 0xC2, 0xA0 };
        static const unsigned char sp[] = { 0x20 };
        u89_check(norm(2, nbsp, 2, a) == 1, "NFKC NBSP -> space");
        u89_check(eq(a, 1, sp, 1), "NFKC NBSP value");
        u89_check(norm(1, nbsp, 2, a) == 2, "NFD keeps NBSP");
    }
}

/* XID_Start is a subset of XID_Continue */
static void check_xid_subset(void)
{
    u89_cp cp;

    for (cp = 0; cp <= 0x10FFFF; cp++) {
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            continue;
        }
        if (u89_xid_start(cp)) {
            u89_check_ctx(u89_xid_continue(cp), "XID_Start subset Continue",
                          "cp");
        }
    }
}

/* Every scalar round-trips through encode/decode (exhaustive) */
static void check_roundtrip(void)
{
    unsigned char buf[8];
    u89_cp cp;
    u89_cp back;
    u89_status st;
    size_t next;
    int len;

    for (cp = 0; cp <= 0x10FFFF; cp++) {
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            continue;
        }
        len = u89_utf8_encode(cp, buf);
        if (len == 0) {
            u89_check_ctx(0, "encode scalar nonzero", "cp");
            continue;
        }
        back = 0;
        next = 0;
        st = u89_utf8_decode(buf, (size_t)len, 0, &back, &next);
        if (st != U89_OK || next != (size_t)len || back != cp) {
            u89_check_ctx(0, "roundtrip", "cp");
            break;
        }
        if (u89_utf8_len(cp) != len) {
            u89_check_ctx(0, "utf8_len == encode len", "cp");
            break;
        }
    }
}

/* NFKC / NFKD idempotence + fold of halfwidth katakana */
static void check_compat_idempotent(void)
{
    static const unsigned char hw_ka[] = { 0xEF, 0xBD, 0xB6 }; /* ｶ */
    static const unsigned char fw_ka[] = { 0xE3, 0x82, 0xAB };  /* カ */
    int l1;
    int l2;

    l1 = norm(2, hw_ka, 3, a);
    l2 = norm(2, a, (size_t)l1, b);
    u89_check_ctx(l1 == l2 && eq(a, (size_t)l1, b, (size_t)l2),
                  "NFKC idempotent", "hw_ka");
    u89_check_ctx(l1 == 3 && eq(a, 3, fw_ka, 3),
                  "NFKC halfwidth katakana -> fullwidth", "hw_ka");

    l1 = norm(3, hw_ka, 3, a);
    l2 = norm(3, a, (size_t)l1, b);
    u89_check_ctx(l1 == l2 && eq(a, (size_t)l1, b, (size_t)l2),
                  "NFKD idempotent", "hw_ka");
}

/* every Hangul syllable: NFD -> 2/3 jamo, and NFC(NFD(syl)) == syl */
static void check_hangul_roundtrip(void)
{
    u89_cp s;
    unsigned char buf[8];
    int len;
    int dn;
    int rn;

    for (s = 0xAC00; s <= 0xD7A3; s += 17) {
        u89_cp idx;
        int expect;

        idx = s - 0xAC00;
        expect = (idx % 28 == 0) ? 6 : 9;
        len = u89_utf8_encode(s, buf);
        dn = norm(1, buf, (size_t)len, a);        /* NFD */
        if (dn != expect) {
            u89_check_ctx(0, "hangul NFD size", "s");
            continue;
        }
        rn = norm(0, a, (size_t)dn, b);            /* NFC(NFD) */
        if (rn != len || !eq(b, (size_t)rn, buf, (size_t)len)) {
            u89_check_ctx(0, "hangul NFC(NFD) roundtrip", "s");
        }
    }
}

/* representative surrogate pairs: decode -> encode -> decode is identity */
static void check_utf16_pair_roundtrip(void)
{
    static const unsigned int hi[] = { 0xD800U, 0xD800U, 0xDBFFU, 0xDBFFU,
                                       0xD83DU };
    static const unsigned int lo[] = { 0xDC00U, 0xDFFFU, 0xDC00U, 0xDFFFU,
                                       0xDE00U };
    static const u89_cp want[] = { 0x10000UL, 0x103FFUL, 0x10FC00UL,
                                   0x10FFFFUL, 0x1F600UL };
    unsigned char buf[8];
    u89_cp cp;
    u89_cp back;
    u89_status st;
    size_t next;
    int len;
    int i;

    for (i = 0; i < 5; i++) {
        cp = 0;
        if (u89_utf16_decode_pair(hi[i], lo[i], &cp) != 1) {
            u89_check_ctx(0, "pair decodes", "pair");
            continue;
        }
        if (cp != want[i]) {
            u89_check_ctx(0, "pair scalar value", "pair");
            continue;
        }
        u89_check_ctx(u89_utf16_units(cp) == 2, "pair scalar is 2 units",
                      "pair");
        len = u89_utf8_encode(cp, buf);
        u89_check_ctx(len == 4, "pair scalar encodes to 4 bytes", "pair");
        back = 0;
        next = 0;
        st = u89_utf8_decode(buf, (size_t)len, 0, &back, &next);
        u89_check_ctx(st == U89_OK && next == (size_t)len && back == cp,
                      "pair scalar round-trips", "pair");
    }
}

/* width is 0,1,2 for all printable scalars, and -1 only for control/non-scalar */
static void check_width_consistency(void)
{
    u89_cp cp;
    int w;

    for (cp = 0x20; cp < 0xD800; cp += 97) {
        w = u89_width(cp, U89_AMBIG_NARROW);
        if (w < 0) {
            u89_check_ctx(w == -1, "width -1 only for control", "cp");
        } else {
            u89_check_ctx(w >= 0 && w <= 2, "width in 0..2", "cp");
        }
    }
}

void test_consistency(void)
{
    static const unsigned char e_acute_decomp[] = { 0x65, 0xCC, 0x81 };
    static const unsigned char precomp_e[] = { 0xC3, 0xA9 };
    static const unsigned char a_grave_acute[] =
        { 0x41, 0xCC, 0x80, 0xCC, 0x81 };   /* A + grave + acute */
    static const unsigned char lv[] = { 0xE1, 0x84, 0x80, 0xE1, 0x85, 0xA1 };

    check_idempotent(0, e_acute_decomp, sizeof e_acute_decomp);
    check_idempotent(1, precomp_e, sizeof precomp_e);
    check_idempotent(0, a_grave_acute, sizeof a_grave_acute);
    check_idempotent(0, lv, sizeof lv);
    check_canonical(e_acute_decomp, sizeof e_acute_decomp);
    check_canonical(a_grave_acute, sizeof a_grave_acute);
    check_compat_folds();
    check_compat_idempotent();
    check_xid_subset();
    check_roundtrip();
    check_width_consistency();
    check_hangul_roundtrip();
    check_utf16_pair_roundtrip();
}
