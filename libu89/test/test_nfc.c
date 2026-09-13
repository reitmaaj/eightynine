#include <string.h>
#include "test.h"
#include "u89.h"

static unsigned char out[256];
static unsigned char out2[256];
static u89_cp work[512];

static int norm(int mode, const unsigned char *s, size_t n)
{
    return u89_normalize_ex(mode, s, n, out, sizeof out, work, 512);
}

static int bytes_eq(const unsigned char *a, size_t an, const unsigned char *b,
                    size_t bn)
{
    if (an != bn) {
        return 0;
    }
    if (memcmp(a, b, an) != 0) {
        return 0;
    }
    return 1;
}

void test_nfc(void)
{
    /* e-acute decomposed (0x65, 0xCC 0x81) and precomposed (0xC3 0xA9) */
    static const unsigned char decomp_e[] = { 0x65, 0xCC, 0x81 };
    static const unsigned char precomp_e[] = { 0xC3, 0xA9 };

    /* NFC: decomposed -> precomposed */
    u89_check(norm(0, decomp_e, sizeof decomp_e) == 2, "NFC size 2");
    u89_check(bytes_eq(out, 2, precomp_e, 2), "NFC composes é");

    /* NFC idempotence */
    {
        int l1 = norm(0, decomp_e, sizeof decomp_e);
        int l2 = u89_normalize_ex(0, out, (size_t)l1, out2, sizeof out2,
                                  work, 512);
        u89_check(l1 == l2 && bytes_eq(out2, (size_t)l2, precomp_e, 2),
                  "NFC idempotent");
    }

    /* NFD: precomposed -> decomposed */
    u89_check(norm(1, precomp_e, sizeof precomp_e) == 3, "NFD size 3");
    u89_check(bytes_eq(out, 3, decomp_e, sizeof decomp_e), "NFD decomposes é");

    /* NFD idempotence */
    {
        int l1 = norm(1, precomp_e, sizeof precomp_e);
        int l2 = u89_normalize_ex(1, out, (size_t)l1, out2, sizeof out2,
                                  work, 512);
        u89_check(l1 == l2 && bytes_eq(out2, (size_t)l2, decomp_e, 3),
                  "NFD idempotent");
    }

    /* Hangul: compose (L,V) = 가 (U+AC00) and (L,V,T) = 각 (U+AC01) */
    {
        static const unsigned char lv[] = { 0xE1, 0x84, 0x80, 0xE1, 0x85, 0xA1 };
        static const unsigned char lvt[] = { 0xE1, 0x84, 0x80, 0xE1, 0x85, 0xA1,
                                             0xE1, 0x86, 0xA8 };
        static const unsigned char ga[] = { 0xEA, 0xB0, 0x80 };
        static const unsigned char gak[] = { 0xEA, 0xB0, 0x81 };

        u89_check(norm(0, lv, sizeof lv) == 3, "Hangul L+V composes to 3 bytes");
        u89_check(bytes_eq(out, 3, ga, 3), "Hangul LV = U+AC00");
        u89_check(norm(0, lvt, sizeof lvt) == 3, "Hangul L+V+T composes to 3 bytes");
        u89_check(bytes_eq(out, 3, gak, 3), "Hangul LVT = U+AC01");
    }

    /* Hangul decompose: NFD of U+AC01 (각) -> L V T */
    {
        static const unsigned char ga[] = { 0xEA, 0xB0, 0x81 };
        int l = norm(1, ga, 3);
        u89_check(l == 9, "Hangul NFD size 9 (3 jamo)");
    }

    /* NFKC: full-width A (U+FF21) -> A (U+0041) */
    {
        static const unsigned char fw[] = { 0xEF, 0xBC, 0xA1 };
        static const unsigned char a[] = { 0x41 };
        u89_check(norm(2, fw, 3) == 1, "NFKC folds to 1 byte");
        u89_check(bytes_eq(out, 1, a, 1), "NFKC fullwidth A -> A");
        /* NFD must NOT fold full-width (canonical only) */
        u89_check(norm(1, fw, 3) == 3, "NFD keeps fullwidth A");
    }

    /* O. embedded NUL preserved */
    {
        unsigned char s[] = { 0x41, 0x00, 0xC3, 0xA9 };
        u89_check(norm(0, s, 4) == 4, "NFC size with NUL");
        u89_check(out[0] == 0x41 && out[1] == 0x00, "NUL preserved position");
    }

    /* sizing call (dst == NULL) returns required capacity */
    {
        static const unsigned char s[] = { 0x65, 0xCC, 0x81 };
        u89_check(u89_normalize_ex(0, s, 3, 0, 0, work, 512) >= 2, "sizing returns capacity");
    }

    /* leading non-starter must NOT block composition of the following
       starter+combining (UAX #15: LastClass resets at a starter). */
    {
        static const unsigned char in[] = { 0xCC, 0x81, 0x41, 0xCC, 0x80 };
        static const unsigned char want[] = { 0xCC, 0x81, 0xC3, 0x80 };
        int l = norm(0, in, sizeof in);
        u89_check(l == 4, "leading non-starter: NFC size 4");
        u89_check(bytes_eq(out, 4, want, sizeof want),
                  "leading non-starter: A+grave composes");
    }

    /* equal-ccc marks must not reorder, and second stays decomposed */
    {
        static const unsigned char in[] = { 0x41, 0xCC, 0x81, 0xCC, 0x80 };
        static const unsigned char want[] = { 0xC3, 0x81, 0xCC, 0x80 };
        int l = norm(0, in, sizeof in);
        u89_check(l == 4, "equal-ccc: NFC size 4");
        u89_check(bytes_eq(out, 4, want, sizeof want),
                  "equal-ccc: A+acute composes, grave stays");
    }

    /* too-small buffer returns -1 (no overflow) */
    {
        static const unsigned char s[] = { 0x65, 0xCC, 0x81 };
        unsigned char tiny[1];
        u89_check(u89_normalize_ex(0, s, 3, tiny, sizeof tiny, work, 512) == U89_ENOSPC,
                  "tiny buffer returns ENOSPC");
    }

    /* sizing value is exact and usable */
    {
        static const unsigned char s[] = { 0x65, 0xCC, 0x81 };
        int cap = u89_normalize_ex(0, s, 3, 0, 0, work, 512);
        u89_check(cap > 0, "sizing positive");
        if (cap > 0) {
            unsigned char buf[64];
            u89_check(u89_normalize_ex(0, s, 3, buf, (size_t)cap, work, 512) >= 0,
                      "sized buffer accepted");
        }
    }

    /* U+FDFA: compatibility decomposition with repeated characters must not
       be truncated (global seen-set bug). */
    {
        static const unsigned char fdfa[] = { 0xEF, 0xB7, 0xBA };
        static const unsigned char want[] = {
            0xD8, 0xB5, 0xD9, 0x84, 0xD9, 0x89, 0x20, 0xD8, 0xA7, 0xD9,
            0x84, 0xD9, 0x84, 0xD9, 0x87, 0x20, 0xD8, 0xB9, 0xD9, 0x84,
            0xD9, 0x8A, 0xD9, 0x87, 0x20, 0xD9, 0x88, 0xD8, 0xB3, 0xD9,
            0x84, 0xD9, 0x85 };
        int l = norm(3, fdfa, 3);
        u89_check(l == 33, "NFKD U+FDFA size 33");
        u89_check(bytes_eq(out, 33, want, sizeof want),
                  "NFKD U+FDFA full decomposition");
    }

    /* U+2A74 (double colon equals): repeated ':' must both survive NFKD. */
    {
        static const unsigned char dce[] = { 0xE2, 0xA9, 0xB4 };
        static const unsigned char want[] = { 0x3A, 0x3A, 0x3D };
        int l = norm(3, dce, 3);
        u89_check(l == 3, "NFKD U+2A74 size 3");
        u89_check(bytes_eq(out, 3, want, 3), "NFKD U+2A74 = '::='");
    }

    /* U+1F06 (multi-step canonical composite) must recompose under NFC. */
    {
        static const unsigned char gr[] = { 0xE1, 0xBC, 0x86 };
        u89_check(norm(0, gr, 3) == 3, "NFC U+1F06 size 3");
        u89_check(bytes_eq(out, 3, gr, 3), "NFC U+1F06 recomposes");
    }

    /* Hangul L and V separated by a combining mark must NOT compose. */
    {
        static const unsigned char lmv[] =
            { 0xE1, 0x84, 0x80, 0xCC, 0x81, 0xE1, 0x85, 0xA1 }; /* L mark V */
        int l = norm(0, lmv, sizeof lmv);
        u89_check(l == 8, "L+mark+V NFC size 8 (no composition)");
    }

    /* Bengali vowel signs: U+09C7 + U+09BE -> U+09CB under NFC. Both carry
       ccc 0, so the old compose() generic path (which only ran when the second
       member had nonzero ccc) never composed them. */
    {
        static const unsigned char in[] = { 0xE0, 0xA7, 0x87, 0xE0, 0xA6, 0xBE };
        static const unsigned char want[] = { 0xE0, 0xA7, 0x8B };
        int l = norm(0, in, sizeof in);
        u89_check(l == 3, "Bengali NFC size 3");
        u89_check(bytes_eq(out, 3, want, sizeof want),
                  "Bengali 09C7+09BE composes to 09CB");
    }
}
