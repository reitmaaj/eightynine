/* test_inet16.c - differential, boundary, and verification coverage for
 * the RFC 1071 Internet checksum. */

#include "ref_crc.h"
#include "test.h"

#define CORPUS_MAX 1024

static unsigned char corpus[CORPUS_MAX];

static void check_lengths(const char *what)
{
    size_t len;

    for (len = 0; len <= CORPUS_MAX; ++len)
    {
        cksum89_test_u16_at(cksum89_inet16(corpus, len),
                            ref_inet16(corpus, len), what, (unsigned long)len);
    }
}

static void check_partition(const unsigned char *data, const size_t *chunks,
                            size_t count, const char *what)
{
    cksum89_inet16_ctx ctx;
    size_t offset;
    size_t i;

    cksum89_inet16_init(&ctx);
    offset = 0;
    for (i = 0; i < count; ++i)
    {
        cksum89_inet16_update(&ctx, data + offset, chunks[i]);
        offset = offset + chunks[i];
    }
    cksum89_test_u16(cksum89_inet16_final(&ctx), cksum89_inet16(data, offset),
                     what);
}

static void check_boundaries(void)
{
    static const unsigned char data[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                           0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                           0x0c, 0x0d, 0x0e, 0x0f};
    static const size_t p11[] = {1, 1};
    static const size_t p12[] = {1, 2};
    static const size_t p21[] = {2, 1};
    static const size_t p33[] = {3, 3};
    static const size_t p111[] = {1, 1, 1};
    static const size_t p5271[] = {5, 2, 7, 1};
    static const size_t p22[] = {2, 2};
    static const size_t p1111[] = {1, 1, 1, 1};
    static const size_t p122[] = {1, 2, 2};
    static const size_t p11111[] = {1, 1, 1, 1, 1};

    check_partition(data, p11, 2, "inet16 partition 1+1");
    check_partition(data, p12, 2, "inet16 partition 1+2");
    check_partition(data, p21, 2, "inet16 partition 2+1");
    check_partition(data, p33, 2, "inet16 partition 3+3");
    check_partition(data, p111, 3, "inet16 partition 1+1+1");
    check_partition(data, p5271, 4, "inet16 partition 5+2+7+1");
    check_partition(data, p22, 2, "inet16 even total even chunks");
    check_partition(data, p1111, 4, "inet16 even total odd chunks");
    check_partition(data, p122, 3, "inet16 odd total even chunks");
    check_partition(data, p11111, 5, "inet16 odd total odd chunks");
}

static void check_verify(const unsigned char *data, size_t len)
{
    unsigned char framed[128];
    cksum89_u16 sum;
    size_t i;

    sum = cksum89_inet16(data, len);
    for (i = 0; i < len; ++i)
    {
        framed[i] = data[i];
    }
    if ((len & 1u) == 0u)
    {
        framed[len] = (unsigned char)(sum >> 8);
        framed[len + 1] = (unsigned char)(sum & 0xffu);
        cksum89_test_u16(cksum89_inet16(framed, len + 2), 0x0000u,
                         "inet16 verify even");
    }
    else
    {
        framed[len] = 0;
        framed[len + 1] = (unsigned char)(sum >> 8);
        framed[len + 2] = (unsigned char)(sum & 0xffu);
        cksum89_test_u16(cksum89_inet16(framed, len + 3), 0x0000u,
                         "inet16 verify odd");
    }
}

static void check_verification(void)
{
    size_t len;

    for (len = 0; len <= 32; ++len)
    {
        check_verify(corpus, len);
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

void test_inet16(void)
{
    check_boundaries();
    fill_zero();
    check_lengths("inet16 zero");
    fill_ones();
    check_lengths("inet16 ones");
    fill_incrementing();
    check_lengths("inet16 incrementing");
    fill_decrementing();
    check_lengths("inet16 decrementing");
    fill_pseudorandom();
    check_lengths("inet16 pseudorandom");
    check_verification();
}
