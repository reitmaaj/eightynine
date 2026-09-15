#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "test.h"
#include "u89.h"

/* Check cluster count, forward/backward traversal, inverse relation, and the
   boundary predicate for one string. */
static void check_clusters(const unsigned char *s, size_t n, size_t want,
                           const char *what)
{
    char ctx[96];
    size_t fwd[64];
    size_t bwd[64];
    size_t nf;
    size_t nb;
    size_t p;
    size_t i;
    int ok;

    sprintf(ctx, "%s", what);
    nf = 0;
    p = 0;
    ok = 1;
    while (p < n && nf < 64) {
        size_t q = u89_grapheme_next(s, n, p);
        if (q <= p) {
            ok = 0;
            break;
        }
        fwd[nf] = q;
        nf++;
        p = q;
    }
    u89_check_ctx(ok == 1 && p == n, "forward traversal reaches end", ctx);
    u89_check_ctx(nf == want, "cluster count", ctx);

    nb = 0;
    p = n;
    ok = 1;
    while (p > 0 && nb < 64) {
        size_t q = u89_grapheme_prev(s, n, p);
        if (q >= p) {
            ok = 0;
            break;
        }
        bwd[nb] = q;
        nb++;
        p = q;
    }
    u89_check_ctx(ok == 1 && p == 0, "backward traversal reaches start", ctx);
    u89_check_ctx(nb == want, "cluster count backward", ctx);

    ok = 1;
    if (nf != nb) {
        ok = 0;
    }
    for (i = 0; ok == 1 && i + 1 < nf; i++) {
        if (fwd[i] != bwd[nf - 2 - i]) {
            ok = 0;
        }
    }
    u89_check_ctx(ok == 1, "forward/backward boundaries agree", ctx);

    p = 0;
    ok = 1;
    for (i = 0; i < nf; i++) {
        if (u89_grapheme_prev(s, n, fwd[i]) != p) {
            ok = 0;
            break;
        }
        p = fwd[i];
    }
    u89_check_ctx(ok == 1, "prev(next(p)) == p", ctx);

    p = 0;
    ok = 1;
    while (p <= n) {
        int b = u89_grapheme_boundary(s, n, p);
        int is_b = 0;
        if (p == 0 || p == n) {
            is_b = 1;
        }
        for (i = 0; i < nf; i++) {
            if (fwd[i] == p) {
                is_b = 1;
            }
        }
        if (b != is_b) {
            ok = 0;
            break;
        }
        if (p == n) {
            break;
        }
        p++;
    }
    u89_check_ctx(ok == 1, "boundary predicate agrees", ctx);
}

static unsigned long rnd_next(unsigned long x)
{
#if ULONG_MAX > 0xFFFFFFFFUL
    return x * 6364136223846793005UL + 1442695040888963407UL;
#else
    return x * 1664525UL + 1013904223UL;
#endif
}

static const u89_cp rnd_pool[] = {
    0x0041, 0x0020, 0x000D, 0x000A, 0x0009, 0x0301, 0x0903, 0x0600,
    0x094D, 0x0915, 0x0937, 0x200D, 0xFE0F, 0x1F468, 0x1F600, 0x1F3FB,
    0x1F1E6, 0x1F1E7, 0x1100, 0x1161, 0x11A8, 0xAC00, 0x1F6D1
};

#define RND_POOL_N (sizeof rnd_pool / sizeof rnd_pool[0])

/* Forward and backward boundary lists must be exact reverses, the boundary
   predicate must match them, and next/prev at arbitrary byte positions must
   agree with the boundary set. */
static int random_case_ok(unsigned long *seed)
{
    unsigned char s[64];
    size_t bounds[32];
    size_t back[32];
    size_t n;
    size_t len;
    size_t i;
    size_t p;
    size_t q;
    size_t k;

    *seed = rnd_next(*seed);
    len = (size_t)((*seed >> 17) % 12);
    n = 0;
    for (i = 0; i < len; i++) {
        int enc;

        *seed = rnd_next(*seed);
        enc = u89_utf8_encode(rnd_pool[(*seed >> 13) % RND_POOL_N], s + n);
        if (enc == 0) {
            return 0;
        }
        n = n + (size_t)enc;
    }
    k = 0;
    p = 0;
    while (p < n) {
        q = u89_grapheme_next(s, n, p);
        if (q <= p || q > n || k >= 32) {
            return 0;
        }
        bounds[k] = q;
        k++;
        p = q;
    }
    if (p != n) {
        return 0;
    }
    back[0] = n;
    p = n;
    i = 0;
    while (p > 0) {
        q = u89_grapheme_prev(s, n, p);
        if (q >= p || i + 1 >= 32) {
            return 0;
        }
        back[i + 1] = q;
        i++;
        p = q;
    }
    if (p != 0 || i != k) {
        return 0;
    }
    for (i = 0; i <= k; i++) {
        size_t f;

        f = 0;
        if (i > 0) {
            f = bounds[i - 1];
        }
        if (f != back[k - i]) {
            return 0;
        }
    }
    for (p = 0; p <= n; p++) {
        int want;
        size_t want_next;
        size_t want_prev;

        want = 0;
        if (p == 0 || p == n) {
            want = 1;
        }
        want_next = n;
        want_prev = 0;
        for (i = 0; i < k; i++) {
            if (bounds[i] == p) {
                want = 1;
            }
            if (bounds[i] > p && want_next == n) {
                want_next = bounds[i];
            }
            if (bounds[i] < p) {
                want_prev = bounds[i];
            }
        }
        if (u89_grapheme_boundary(s, n, p) != want) {
            return 0;
        }
        if (u89_grapheme_next(s, n, p) != want_next) {
            return 0;
        }
        if (u89_grapheme_prev(s, n, p) != want_prev) {
            return 0;
        }
    }
    return 1;
}

static void check_random(void)
{
    unsigned long seed;
    int i;
    int ok;

    seed = 1;
    ok = 1;
    for (i = 0; i < 1000; i++) {
        if (!random_case_ok(&seed)) {
            ok = 0;
            break;
        }
    }
    u89_check(ok == 1, "randomized traversal invariants");
}

void test_grapheme(void)
{
    static const unsigned char EMPTY[] = { 0 };
    static const unsigned char ASCII[] = { 'a', 'b', 'c' };
    static const unsigned char E_ACUTE[] = { 'e', 0xCC, 0x81 };
    static const unsigned char MULTI_MARK[] = { 'a', 0xCC, 0x81, 0xCC, 0x82 };
    static const unsigned char CJK[] = { 0xE4, 0xB8, 0xAD, 'x' };
    static const unsigned char CRLF[] = { 0x0D, 0x0A };
    static const unsigned char A_CRLF_B[] = { 'a', 0x0D, 0x0A, 'b' };
    static const unsigned char CR_MARK[] = { 0x0D, 0xCC, 0x81 };
    static const unsigned char MODIFIER[] = {
        0xF0, 0x9F, 0x91, 0x8D, 0xF0, 0x9F, 0x8F, 0xBD
    };
    static const unsigned char ZWJ_FAMILY[] = {
        0xF0, 0x9F, 0x91, 0xA8, 0xE2, 0x80, 0x8D,
        0xF0, 0x9F, 0x91, 0xA9, 0xE2, 0x80, 0x8D,
        0xF0, 0x9F, 0x91, 0xA7
    };
    static const unsigned char ZWJ_NONPICT[] = {
        0xF0, 0x9F, 0x91, 0xA9, 0xE2, 0x80, 0x8D, 'a'
    };
    static const unsigned char FLAG[] = {
        0xF0, 0x9F, 0x87, 0xAB, 0xF0, 0x9F, 0x87, 0xAE
    };
    static const unsigned char RI3[] = {
        0xF0, 0x9F, 0x87, 0xAB, 0xF0, 0x9F, 0x87, 0xAE,
        0xF0, 0x9F, 0x87, 0xA6
    };
    static const unsigned char HANGUL[] = {
        0xE1, 0x84, 0x80, 0xE1, 0x85, 0xA1, 0xE1, 0x86, 0xA8
    };
    static const unsigned char HANGUL_LVT[] = { 0xEA, 0xB0, 0x80, 0xE1, 0x86, 0xA8 };
    static const unsigned char PREPEND[] = { 0xD8, 0x80, 'a' };
    static const unsigned char SPACINGMARK[] = { 'a', 0xE0, 0xA4, 0x83 };
    static const unsigned char CONJUNCT[] = {
        0xE0, 0xA4, 0x95, 0xE0, 0xA5, 0x8D, 0xE0, 0xA4, 0xB7
    };
    static const unsigned char VS16[] = { 0xE2, 0x9D, 0xA4, 0xEF, 0xB8, 0x8F };
    static const unsigned char ZWJ_VS16_PICT[] = {
        0xF0, 0x9F, 0x91, 0xA8, 0xE2, 0x80, 0x8D, 0xEF, 0xB8, 0x8F,
        0xF0, 0x9F, 0x98, 0x80
    };
    static const unsigned char ZWJ_VS16_ZWJ_PICT[] = {
        0xF0, 0x9F, 0x91, 0xA8, 0xE2, 0x80, 0x8D, 0xEF, 0xB8, 0x8F,
        0xE2, 0x80, 0x8D, 0xF0, 0x9F, 0x98, 0x80
    };
    static const unsigned char PICT_VS16_ZWJ_PICT[] = {
        0xF0, 0x9F, 0x91, 0xA8, 0xEF, 0xB8, 0x8F, 0xE2, 0x80, 0x8D,
        0xF0, 0x9F, 0x98, 0x80
    };

    /* GR-11/GR-12: empty and endpoint behavior */
    u89_check(u89_grapheme_next(EMPTY, 0, 0) == 0, "empty next");
    u89_check(u89_grapheme_prev(EMPTY, 0, 0) == 0, "empty prev");
    u89_check(u89_grapheme_boundary(EMPTY, 0, 0) == 1, "empty boundary");
    check_clusters(EMPTY, 0, 0, "empty");

    check_clusters(ASCII, sizeof ASCII, 3, "ascii");
    check_clusters(E_ACUTE, sizeof E_ACUTE, 1, "e + acute");
    check_clusters(MULTI_MARK, sizeof MULTI_MARK, 1, "multiple marks");
    check_clusters(CJK, sizeof CJK, 2, "cjk + ascii");
    check_clusters(CRLF, sizeof CRLF, 1, "crlf");
    check_clusters(A_CRLF_B, sizeof A_CRLF_B, 3, "a crlf b");
    check_clusters(CR_MARK, sizeof CR_MARK, 2, "cr + mark");
    check_clusters(MODIFIER, sizeof MODIFIER, 1, "emoji modifier");
    check_clusters(ZWJ_FAMILY, sizeof ZWJ_FAMILY, 1, "zwj family");
    check_clusters(ZWJ_NONPICT, sizeof ZWJ_NONPICT, 2, "zwj non-pictograph");
    check_clusters(FLAG, sizeof FLAG, 1, "flag pair");
    check_clusters(RI3, sizeof RI3, 2, "three regional indicators");
    check_clusters(HANGUL, sizeof HANGUL, 1, "hangul L V T");
    check_clusters(HANGUL_LVT, sizeof HANGUL_LVT, 1, "hangul LV T");
    check_clusters(PREPEND, sizeof PREPEND, 1, "prepend + ascii");
    check_clusters(SPACINGMARK, sizeof SPACINGMARK, 1, "spacing mark");
    check_clusters(CONJUNCT, sizeof CONJUNCT, 1, "indic conjunct");
    check_clusters(VS16, sizeof VS16, 1, "emoji + vs16");
    check_clusters(ZWJ_VS16_PICT, sizeof ZWJ_VS16_PICT, 2,
                   "zwj + vs16 breaks gb11");
    check_clusters(ZWJ_VS16_ZWJ_PICT, sizeof ZWJ_VS16_ZWJ_PICT, 2,
                   "zwj extend zwj breaks gb11");
    check_clusters(PICT_VS16_ZWJ_PICT, sizeof PICT_VS16_ZWJ_PICT, 1,
                   "pict extend zwj pict joins");

    /* Mid-scalar positions are never boundaries. */
    u89_check(u89_grapheme_boundary(E_ACUTE, sizeof E_ACUTE, 1) == 0,
              "mid-scalar not a boundary");
    u89_check(u89_grapheme_prev(E_ACUTE, sizeof E_ACUTE, 1) == 0,
              "prev from mid-scalar");
    u89_check(u89_grapheme_next(E_ACUTE, sizeof E_ACUTE, 1) == sizeof E_ACUTE,
              "next from mid-scalar");

    check_random();
}
