/* test_dyn.c - dynamic (Salson) editor: the maintained transform must equal a
 * freshly recomputed bwt89_bwt after every edit, round-trip via bwt89_ibwt,
 * and reject invalid edits without corrupting the handle. */
#include <stdlib.h>
#include <string.h>

#include "test.h"
#include <bwt89.h>

/* Compare the handle transform against a from-scratch recompute of shadow.
 * Buffers are sized to `len` so documents of any length (not just a fixed cap)
 * are verified without overflowing. */
static void check_against_recompute(struct bwt89_ed *ed,
                                    const unsigned char *shadow, size_t len)
{
    unsigned char *eb;
    unsigned char *rb;
    unsigned char *rt;
    size_t ei;
    size_t ri;
    enum bwt89_status st;

    eb = (unsigned char *)malloc(len ? len : 1);
    rb = (unsigned char *)malloc(len ? len : 1);
    rt = (unsigned char *)malloc(len ? len : 1);
    if (eb == NULL || rb == NULL || rt == NULL)
    {
        free(eb);
        free(rb);
        free(rt);
        ++test_failures;
        return;
    }
    st = bwt89_ed_bwt(ed, &ei, eb);
    CHECK(st == BWT89_OK);
    st = bwt89_bwt(shadow, len, &ri, rb);
    CHECK(st == BWT89_OK);
    CHECK(ei == ri);
    CHECK(memcmp(eb, rb, len) == 0);
    st = bwt89_ibwt(eb, len, ei, rt);
    CHECK(st == BWT89_OK);
    CHECK(memcmp(rt, shadow, len) == 0);
    free(eb);
    free(rb);
    free(rt);
}

static unsigned long rng = 88172645463325252UL;

static int rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (int)(rng % 1000000000UL);
}

/* Compare the handle transform against a from-scratch recompute of shadow. */
static void check_open_and_edits(const unsigned char *init, size_t n, int iters)
{
    struct bwt89_ed *ed;
    unsigned char *shadow;
    size_t len;
    size_t pos;
    unsigned char c;
    int it;
    enum bwt89_status st;

    /* worst-case length is the initial n plus one per iteration */
    shadow = (unsigned char *)malloc(n + (size_t)iters + 2);
    if (shadow == NULL)
    {
        ++test_failures;
        return;
    }
    memcpy(shadow, init, n);
    len = n;
    st = bwt89_ed_open(&ed, init, n);
    CHECK(st == BWT89_OK);
    CHECK(ed != NULL);
    if (ed == NULL)
    {
        free(shadow);
        return;
    }
    check_against_recompute(ed, shadow, len);

    for (it = 0; it < iters; ++it)
    {
        int kind = rnd() % 3;
        if (kind == 0 || len == 0)
        {
            /* insert at pos in [0,len] */
            pos = (size_t)(rnd() % ((int)len + 1));
            c = (unsigned char)(rnd() % 256);
            st = bwt89_ed_insert(ed, pos, c);
            CHECK(st == BWT89_OK);
            if (pos < len)
            {
                memmove(shadow + pos + 1, shadow + pos, len - pos);
            }
            shadow[pos] = c;
            ++len;
        }
        else if (kind == 1)
        {
            /* delete at pos in [0,len) */
            pos = (size_t)(rnd() % (int)len);
            st = bwt89_ed_delete(ed, pos);
            CHECK(st == BWT89_OK);
            memmove(shadow + pos, shadow + pos + 1, len - pos - 1);
            --len;
        }
        else
        {
            /* substitute at pos in [0,len) */
            pos = (size_t)(rnd() % (int)len);
            c = (unsigned char)(rnd() % 256);
            st = bwt89_ed_substitute(ed, pos, c);
            CHECK(st == BWT89_OK);
            shadow[pos] = c;
        }
        check_against_recompute(ed, shadow, len);
    }
    st = bwt89_ed_close(ed);
    CHECK(st == BWT89_OK);
    free(shadow);
}

/* Grow a document well past any fixed buffer cap (exercising capacity growth)
 * and verify the transform equals recompute throughout. */
static void check_long_document(void)
{
    struct bwt89_ed *ed;
    unsigned char *shadow;
    size_t len;
    size_t cap;
    int i;
    unsigned char seed[1];
    enum bwt89_status st;

    cap = 600;
    shadow = (unsigned char *)malloc(cap + 8);
    if (shadow == NULL)
    {
        ++test_failures;
        return;
    }
    seed[0] = 0;
    st = bwt89_ed_open(&ed, seed, 0);
    CHECK(st == BWT89_OK);
    if (ed == NULL)
    {
        free(shadow);
        return;
    }
    len = 0;
    for (i = 0; i < 600; ++i)
    {
        unsigned char c = (unsigned char)('a' + (i % 7));
        st = bwt89_ed_insert(ed, len, c);
        CHECK(st == BWT89_OK);
        shadow[len] = c;
        ++len;
        check_against_recompute(ed, shadow, len);
    }
    for (i = 0; i < 300; ++i)
    {
        size_t pos = (size_t)((unsigned int)rnd() % (unsigned int)len);
        unsigned char c = (unsigned char)('0' + (rnd() % 10));
        st = bwt89_ed_substitute(ed, pos, c);
        CHECK(st == BWT89_OK);
        shadow[pos] = c;
        check_against_recompute(ed, shadow, len);
    }
    while (len > 0)
    {
        st = bwt89_ed_delete(ed, len - 1);
        CHECK(st == BWT89_OK);
        --len;
        check_against_recompute(ed, shadow, len);
    }
    st = bwt89_ed_close(ed);
    CHECK(st == BWT89_OK);
    free(shadow);
}

static void check_rejections(void)
{
    struct bwt89_ed *ed;
    unsigned char one[1];
    unsigned char out[8];
    size_t len;
    size_t idx;
    enum bwt89_status st;

    one[0] = 'a';
    st = bwt89_ed_open(NULL, one, 1);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ed_open(&ed, NULL, 1);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ed_open(&ed, one, 1);
    CHECK(st == BWT89_OK);

    /* insert beyond the end is rejected */
    st = bwt89_ed_insert(ed, 2, 'b');
    CHECK(st == BWT89_BAD_EDIT);
    /* delete / substitute past the end are rejected */
    st = bwt89_ed_delete(ed, 1);
    CHECK(st == BWT89_BAD_EDIT);
    st = bwt89_ed_substitute(ed, 1, 'c');
    CHECK(st == BWT89_BAD_EDIT);
    /* handle unchanged after all rejected edits: still "a" */
    check_against_recompute(ed, one, 1);

    st = bwt89_ed_insert(NULL, 0, 'x');
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ed_delete(NULL, 0);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ed_substitute(NULL, 0, 'x');
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ed_length(NULL, &len);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ed_bwt(NULL, &idx, out);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ed_length(ed, NULL);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ed_bwt(ed, NULL, out);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ed_bwt(ed, &idx, NULL);
    CHECK(st == BWT89_NULL_ARG);
    st = bwt89_ed_close(NULL);
    CHECK(st == BWT89_NULL_ARG);

    st = bwt89_ed_close(ed);
    CHECK(st == BWT89_OK);

    /* delete on an empty handle is rejected */
    st = bwt89_ed_open(&ed, one, 0);
    CHECK(st == BWT89_OK);
    st = bwt89_ed_delete(ed, 0);
    CHECK(st == BWT89_BAD_EDIT);
    st = bwt89_ed_substitute(ed, 0, 'z');
    CHECK(st == BWT89_BAD_EDIT);
    st = bwt89_ed_length(ed, &len);
    CHECK(st == BWT89_OK);
    CHECK(len == 0);
    st = bwt89_ed_close(ed);
    CHECK(st == BWT89_OK);
}

int main(void)
{
    unsigned char buf[64];
    unsigned char sub[8];
    unsigned char nul[4];
    size_t i;
    int it;

    nul[0] = 0;
    nul[1] = 'x';
    nul[2] = 0;
    nul[3] = 'y';

    sub[0] = 0;
    check_open_and_edits(sub, 0, 120); /* empty */
    sub[0] = 'a';
    check_open_and_edits(sub, 1, 120); /* single byte */
    memcpy(buf, "banana", 6);
    check_open_and_edits(buf, 6, 200);
    check_open_and_edits(nul, 4, 200); /* embedded NULs */
    memcpy(buf, "abc", 3);
    check_open_and_edits(buf, 3, 400); /* small alphabet stress */

    for (it = 0; it < 8; ++it)
    {
        size_t n = 1 + (size_t)(rnd() % 40);
        for (i = 0; i < n; ++i)
        {
            buf[i] = (unsigned char)('a' + (rnd() % 3));
        }
        check_open_and_edits(buf, n, 150);
    }

    check_rejections();
    check_long_document();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
