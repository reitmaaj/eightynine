#include <string.h>
#include "test.h"
#include "u89.h"

static unsigned char out[512];

static int fold(int mode, const unsigned char *s, size_t n)
{
    return u89_casefold(mode, s, n, out, sizeof out);
}

static int beq(const unsigned char *a, size_t an, const unsigned char *b,
               size_t bn)
{
    if (an != bn) {
        return 0;
    }
    return memcmp(a, b, an) == 0;
}

void test_casefold(void)
{
    /* ASCII */
    {
        static const unsigned char a[] = { 0x41 };
        static const unsigned char lo[] = { 0x61 };
        int l = fold(0, a, 1);
        u89_check(l == 1 && beq(out, 1, lo, 1), "fold 'A' -> 'a'");
    }
    /* capital I: default -> i, Turkic -> dotless i (U+0131) */
    {
        static const unsigned char i_[] = { 0x49 };
        static const unsigned char lti[] = { 0xC4, 0xB1 };
        int l = fold(0, i_, 1);
        u89_check(l == 1 && out[0] == 0x69, "fold 'I' default -> 'i'");
        l = fold(1, i_, 1);
        u89_check(l == 2 && beq(out, 2, lti, 2), "fold 'I' Turkic -> dotless i");
    }
    /* sharp s: default full fold -> "ss" */
    {
        static const unsigned char sz[] = { 0xC3, 0x9F };
        static const unsigned char ss[] = { 0x73, 0x73 };
        int l = fold(0, sz, 2);
        u89_check(l == 2 && beq(out, 2, ss, 2), "fold sharp-s -> ss");
    }
    /* I-with-dot (U+0130): default -> "i" + U+0307; Turkic -> "i" */
    {
        static const unsigned char idot[] = { 0xC4, 0xB0 };
        static const unsigned char i_dot[] = { 0x69, 0xCC, 0x87 };
        static const unsigned char ii[] = { 0x69 };
        int l = fold(0, idot, 2);
        u89_check(l == 3 && beq(out, 3, i_dot, 3), "fold U+0130 default");
        l = fold(1, idot, 2);
        u89_check(l == 1 && beq(out, 1, ii, 1), "fold U+0130 Turkic");
    }
    /* whole word "FUSS" */
    {
        static const unsigned char fuss[] = { 0x46, 0x55, 0x53, 0x53 };
        static const unsigned char fusz[] = { 0x46, 0x75, 0xC3, 0x9F };
        static const unsigned char want[] = { 0x66, 0x75, 0x73, 0x73 };
        int l = fold(0, fuss, 4);
        u89_check(l == 4 && beq(out, 4, want, 4), "fold FUSS");
        /* F + u + sharp-s folds to "fuss" (length grows: 4 bytes in, 4 out) */
        l = fold(0, fusz, 4);
        u89_check(l == 4 && beq(out, 4, want, 4), "fold 'Fu'+sz -> fuss");
    }
    /* bad mode and malformed input rejected */
    {
        static const unsigned char ok[] = { 0x41 };
        static const unsigned char bad[] = { 0xC0, 0xAF };
        u89_check(u89_casefold(2, ok, 1, out, sizeof out) == U89_EINVAL,
                  "casefold bad mode");
        u89_check(u89_casefold(0, bad, 2, out, sizeof out) == U89_EUTF8,
                  "casefold malformed input");
    }
    /* sizing query */
    {
        static const unsigned char sz[] = { 0xC3, 0x9F };
        u89_check(u89_casefold(0, sz, 2, NULL, 0) == 2, "casefold sizing");
    }
}
