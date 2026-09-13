/* test_crc64_nvme.c - differential CRC-64/NVMe against the reference model
 * over deterministic corpora. */

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
        cksum89_u64 want;

        one[0] = (unsigned char)i;
        want = ref_crc64_nvme(one, 1);
        cksum89_test_u64_at(cksum89_crc64_nvme(one, 1), want.hi, want.lo,
                            "crc64 nvme one-byte", (unsigned long)i);
    }
}

static void check_lengths(const char *what)
{
    size_t len;

    for (len = 0; len <= CORPUS_MAX; ++len)
    {
        cksum89_u64 want;

        want = ref_crc64_nvme(corpus, len);
        cksum89_test_u64_at(cksum89_crc64_nvme(corpus, len), want.hi, want.lo,
                            what, (unsigned long)len);
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

void test_crc64_nvme(void)
{
    check_bytes();
    fill_zero();
    check_lengths("crc64 nvme zero");
    fill_ones();
    check_lengths("crc64 nvme ones");
    fill_incrementing();
    check_lengths("crc64 nvme incrementing");
    fill_decrementing();
    check_lengths("crc64 nvme decrementing");
    fill_pseudorandom();
    check_lengths("crc64 nvme pseudorandom");
}
