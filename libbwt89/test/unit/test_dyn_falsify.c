/* test_dyn_falsify.c - exhaustive single-edit falsification (Probe 1/2/3 at
 * unit granularity). For every short text in a small alphabet, opening a
 * handle and applying ONE delete or ONE insert at every position must leave
 * the handle's transform byte-for-byte equal to a fresh bwt89_bwt of the new
 * text, with equal index, and must round-trip via bwt89_ibwt.
 *
 * GREEN today because the baseline ed_bwt delegates to bwt89_bwt. When the
 * Salson incremental maintenance lands, this test becomes the gate that any
 * per-edit update rule must keep green. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

#define MAXLEN 9

/* Enumerate every text of `len` bytes over alphabet [0, alen) via a mixed-radix
 * counter in buf[0..len-1]; call cb(buf,len) once per text. */
typedef void (*text_cb)(const unsigned char *, size_t);

static void for_each_text(unsigned char *buf, size_t len, size_t alen, size_t i,
                          text_cb cb)
{
    size_t v;
    for (v = 0; v < alen; ++v)
    {
        buf[i] = (unsigned char)v;
        if (i + 1 == len)
        {
            cb(buf, len);
        }
        else
        {
            for_each_text(buf, len, alen, i + 1, cb);
        }
    }
}

/* Compare the handle transform against a fresh recompute of `text`; also
 * round-trip the handle transform through bwt89_ibwt. */
static void check_one(const unsigned char *text, size_t len)
{
    struct bwt89_ed *ed;
    unsigned char eb[MAXLEN];
    unsigned char rb[MAXLEN];
    unsigned char rt[MAXLEN];
    size_t ei;
    size_t ri;
    enum bwt89_status st;

    st = bwt89_ed_open(&ed, text, len);
    CHECK(st == BWT89_OK);
    if (ed == NULL)
    {
        return;
    }
    st = bwt89_ed_bwt(ed, &ei, eb);
    CHECK(st == BWT89_OK);
    st = bwt89_bwt(text, len, &ri, rb);
    CHECK(st == BWT89_OK);
    CHECK(ei == ri);
    CHECK(memcmp(eb, rb, len) == 0);
    if (len > 0)
    {
        st = bwt89_ibwt(eb, len, ei, rt);
        CHECK(st == BWT89_OK);
        CHECK(memcmp(rt, text, len) == 0);
    }
    bwt89_ed_close(ed);
}

static void probe_delete(const unsigned char *text, size_t len)
{
    struct bwt89_ed *ed;
    unsigned char eb[MAXLEN];
    unsigned char rb[MAXLEN];
    unsigned char nt[MAXLEN];
    unsigned char eone[MAXLEN];
    size_t ei;
    size_t ri;
    size_t pos;
    enum bwt89_status st;

    if (len == 0)
    {
        return;
    }
    for (pos = 0; pos < len; ++pos)
    {
        st = bwt89_ed_open(&ed, text, len);
        CHECK(st == BWT89_OK);
        if (ed == NULL)
        {
            return;
        }
        st = bwt89_ed_delete(ed, pos);
        CHECK(st == BWT89_OK);
        memcpy(nt, text, len);
        memmove(nt + pos, nt + pos + 1, len - pos - 1);
        st = bwt89_ed_bwt(ed, &ei, eb);
        CHECK(st == BWT89_OK);
        st = bwt89_bwt(nt, len - 1, &ri, rb);
        CHECK(st == BWT89_OK);
        CHECK(ei == ri);
        CHECK(memcmp(eb, rb, len - 1) == 0);
        if (len > 1)
        {
            st = bwt89_ibwt(eb, len - 1, ei, eone);
            CHECK(st == BWT89_OK);
            CHECK(memcmp(eone, nt, len - 1) == 0);
        }
        bwt89_ed_close(ed);
    }
}

static void probe_insert(const unsigned char *text, size_t len)
{
    struct bwt89_ed *ed;
    unsigned char eb[MAXLEN];
    unsigned char rb[MAXLEN];
    unsigned char nt[MAXLEN];
    unsigned char eone[MAXLEN];
    size_t ei;
    size_t ri;
    size_t pos;
    size_t v;
    enum bwt89_status st;

    for (pos = 0; pos <= len; ++pos)
    {
        for (v = 0; v < 2; ++v)
        {
            st = bwt89_ed_open(&ed, text, len);
            CHECK(st == BWT89_OK);
            if (ed == NULL)
            {
                return;
            }
            st = bwt89_ed_insert(ed, pos, (unsigned char)v);
            CHECK(st == BWT89_OK);
            memcpy(nt, text, len);
            memmove(nt + pos + 1, nt + pos, len - pos);
            nt[pos] = (unsigned char)v;
            st = bwt89_ed_bwt(ed, &ei, eb);
            CHECK(st == BWT89_OK);
            st = bwt89_bwt(nt, len + 1, &ri, rb);
            CHECK(st == BWT89_OK);
            CHECK(ei == ri);
            CHECK(memcmp(eb, rb, len + 1) == 0);
            st = bwt89_ibwt(eb, len + 1, ei, eone);
            CHECK(st == BWT89_OK);
            CHECK(memcmp(eone, nt, len + 1) == 0);
            bwt89_ed_close(ed);
        }
    }
}

static void check_text_delete(const unsigned char *text, size_t len)
{
    check_one(text, len);
    probe_delete(text, len);
}

static void check_text_insert(const unsigned char *text, size_t len)
{
    probe_insert(text, len);
}

int main(void)
{
    unsigned char buf[MAXLEN];
    size_t len;

    check_one((const unsigned char *)"", 0);
    for (len = 1; len <= 8; ++len)
    {
        for_each_text(buf, len, 2, 0, check_text_delete);
    }
    for (len = 1; len <= 7; ++len)
    {
        for_each_text(buf, len, 2, 0, check_text_insert);
    }
    for (len = 1; len <= 4; ++len)
    {
        for_each_text(buf, len, 3, 0, check_text_delete);
        for_each_text(buf, len, 3, 0, check_text_insert);
    }
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
