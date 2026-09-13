/* test_crc32_iso_hdlc.c - differential CRC-32/ISO-HDLC against the
 * reference model over deterministic corpora. */

#include "ref_crc.h"
#include "test.h"

#define CORPUS_MAX 1024

static unsigned char corpus[CORPUS_MAX];

static void check_bytes(void)
{
    unsigned char one[1];
    unsigned int i;

    for (i = 0; i < 256u; ++i)
    {
        one[0] = (unsigned char)i;
        cksum89_test_u32_at(cksum89_crc32_iso_hdlc(one, 1),
                            ref_crc32_iso_hdlc(one, 1), "crc32 iso one-byte",
                            (unsigned long)i);
    }
}

static void check_lengths(const char *what)
{
    size_t len;

    for (len = 0; len <= CORPUS_MAX; ++len)
    {
        cksum89_test_u32_at(cksum89_crc32_iso_hdlc(corpus, len),
                            ref_crc32_iso_hdlc(corpus, len), what,
                            (unsigned long)len);
    }
}

static void fill_zero(void)
{
    size_t i;

    for (i = 0; i < CORPUS_MAX; ++i)
    {
        corpus[i] = 0;
    }
}

static void fill_ones(void)
{
    size_t i;

    for (i = 0; i < CORPUS_MAX; ++i)
    {
        corpus[i] = 0xff;
    }
}

static void fill_incrementing(void)
{
    size_t i;

    for (i = 0; i < CORPUS_MAX; ++i)
    {
        corpus[i] = (unsigned char)(i & 0xffu);
    }
}

static void fill_decrementing(void)
{
    size_t i;

    for (i = 0; i < CORPUS_MAX; ++i)
    {
        corpus[i] = (unsigned char)((CORPUS_MAX - i) & 0xffu);
    }
}

static void fill_pseudorandom(void)
{
    unsigned long state;
    size_t i;

    state = 0x12345678UL;
    for (i = 0; i < CORPUS_MAX; ++i)
    {
        state = ((state * 1103515245UL) + 12345UL) & 0xffffffffUL;
        corpus[i] = (unsigned char)((state >> 16) & 0xffUL);
    }
}

void test_crc32_iso_hdlc(void)
{
    check_bytes();
    fill_zero();
    check_lengths("crc32 iso zero");
    fill_ones();
    check_lengths("crc32 iso ones");
    fill_incrementing();
    check_lengths("crc32 iso incrementing");
    fill_decrementing();
    check_lengths("crc32 iso decrementing");
    fill_pseudorandom();
    check_lengths("crc32 iso pseudorandom");
}
