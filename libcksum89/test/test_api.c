/* test_api.c - context laws, alignment independence, length boundaries,
 * input immutability, and value-domain invariants. */

#include <limits.h>
#include <string.h>

#include "ref_crc.h"
#include "test.h"

#define API_BUF 4200
#define API_PAD 8

static unsigned char backing[API_BUF + API_PAD];

static void fill_backing(void)
{
    size_t i;

    for (i = 0; i < API_BUF + API_PAD; ++i)
    {
        backing[i] = (unsigned char)(((i * 7u) + 3u) & 0xffu);
    }
}

static void check_crc32_iso_laws(void)
{
    cksum89_crc32_iso_hdlc_ctx a;
    cksum89_crc32_iso_hdlc_ctx b;
    cksum89_crc32_iso_hdlc_ctx before;
    cksum89_u32 first;
    cksum89_u32 second;

    cksum89_crc32_iso_hdlc_init(&a);
    cksum89_crc32_iso_hdlc_update(&a, backing, 16);
    before = a;
    first = cksum89_crc32_iso_hdlc_final(&a);
    second = cksum89_crc32_iso_hdlc_final(&a);
    cksum89_test_u32(first, second, "crc32 iso final repeatable");
    cksum89_test_check(memcmp(&a, &before, sizeof a) == 0,
                       "crc32 iso final does not mutate");

    cksum89_crc32_iso_hdlc_init(&a);
    cksum89_crc32_iso_hdlc_update(&a, backing, 5);
    b = a;
    cksum89_crc32_iso_hdlc_update(&a, backing + 5, 7);
    cksum89_crc32_iso_hdlc_update(&b, backing + 5, 3);
    cksum89_test_u32(cksum89_crc32_iso_hdlc_final(&a),
                     cksum89_crc32_iso_hdlc(backing, 12), "crc32 iso branch a");
    cksum89_test_u32(cksum89_crc32_iso_hdlc_final(&b),
                     cksum89_crc32_iso_hdlc(backing, 8), "crc32 iso branch b");

    cksum89_crc32_iso_hdlc_init(&a);
    cksum89_crc32_iso_hdlc_update(&a, backing, 9);
    before = a;
    cksum89_crc32_iso_hdlc_update(&a, NULL, 0);
    cksum89_test_check(memcmp(&a, &before, sizeof a) == 0,
                       "crc32 iso null zero no-op");

    cksum89_crc32_iso_hdlc_init(&a);
    cksum89_crc32_iso_hdlc_update(&a, backing, 20);
    cksum89_crc32_iso_hdlc_init(&a);
    cksum89_crc32_iso_hdlc_update(&a, backing, 5);
    cksum89_test_u32(cksum89_crc32_iso_hdlc_final(&a),
                     cksum89_crc32_iso_hdlc(backing, 5),
                     "crc32 iso init resets");

    cksum89_crc32_iso_hdlc_init(&a);
    cksum89_crc32_iso_hdlc_update(&a, backing, 0);
    cksum89_crc32_iso_hdlc_update(&a, backing, 4);
    cksum89_crc32_iso_hdlc_update(&a, NULL, 0);
    cksum89_crc32_iso_hdlc_update(&a, backing + 4, 6);
    cksum89_crc32_iso_hdlc_update(&a, backing + 10, 0);
    cksum89_test_u32(cksum89_crc32_iso_hdlc_final(&a),
                     cksum89_crc32_iso_hdlc(backing, 10),
                     "crc32 iso zero-length inserts");
}

static void check_crc32c_laws(void)
{
    cksum89_crc32c_ctx a;
    cksum89_crc32c_ctx b;
    cksum89_crc32c_ctx before;
    cksum89_u32 first;
    cksum89_u32 second;

    cksum89_crc32c_init(&a);
    cksum89_crc32c_update(&a, backing, 16);
    before = a;
    first = cksum89_crc32c_final(&a);
    second = cksum89_crc32c_final(&a);
    cksum89_test_u32(first, second, "crc32c final repeatable");
    cksum89_test_check(memcmp(&a, &before, sizeof a) == 0,
                       "crc32c final does not mutate");

    cksum89_crc32c_init(&a);
    cksum89_crc32c_update(&a, backing, 5);
    b = a;
    cksum89_crc32c_update(&a, backing + 5, 7);
    cksum89_crc32c_update(&b, backing + 5, 3);
    cksum89_test_u32(cksum89_crc32c_final(&a), cksum89_crc32c(backing, 12),
                     "crc32c branch a");
    cksum89_test_u32(cksum89_crc32c_final(&b), cksum89_crc32c(backing, 8),
                     "crc32c branch b");

    cksum89_crc32c_init(&a);
    cksum89_crc32c_update(&a, backing, 9);
    before = a;
    cksum89_crc32c_update(&a, NULL, 0);
    cksum89_test_check(memcmp(&a, &before, sizeof a) == 0,
                       "crc32c null zero no-op");

    cksum89_crc32c_init(&a);
    cksum89_crc32c_update(&a, backing, 20);
    cksum89_crc32c_init(&a);
    cksum89_crc32c_update(&a, backing, 5);
    cksum89_test_u32(cksum89_crc32c_final(&a), cksum89_crc32c(backing, 5),
                     "crc32c init resets");

    cksum89_crc32c_init(&a);
    cksum89_crc32c_update(&a, backing, 0);
    cksum89_crc32c_update(&a, backing, 4);
    cksum89_crc32c_update(&a, NULL, 0);
    cksum89_crc32c_update(&a, backing + 4, 6);
    cksum89_crc32c_update(&a, backing + 10, 0);
    cksum89_test_u32(cksum89_crc32c_final(&a), cksum89_crc32c(backing, 10),
                     "crc32c zero-length inserts");
}

static void check_crc64_laws(void)
{
    cksum89_crc64_nvme_ctx a;
    cksum89_crc64_nvme_ctx b;
    cksum89_crc64_nvme_ctx before;
    cksum89_u64 first;
    cksum89_u64 second;
    cksum89_u64 want;

    cksum89_crc64_nvme_init(&a);
    cksum89_crc64_nvme_update(&a, backing, 16);
    before = a;
    first = cksum89_crc64_nvme_final(&a);
    second = cksum89_crc64_nvme_final(&a);
    cksum89_test_u64(first, second.hi, second.lo,
                     "crc64 nvme final repeatable");
    cksum89_test_check(memcmp(&a, &before, sizeof a) == 0,
                       "crc64 nvme final does not mutate");

    cksum89_crc64_nvme_init(&a);
    cksum89_crc64_nvme_update(&a, backing, 5);
    b = a;
    cksum89_crc64_nvme_update(&a, backing + 5, 7);
    cksum89_crc64_nvme_update(&b, backing + 5, 3);
    want = cksum89_crc64_nvme(backing, 12);
    cksum89_test_u64(cksum89_crc64_nvme_final(&a), want.hi, want.lo,
                     "crc64 nvme branch a");
    want = cksum89_crc64_nvme(backing, 8);
    cksum89_test_u64(cksum89_crc64_nvme_final(&b), want.hi, want.lo,
                     "crc64 nvme branch b");

    cksum89_crc64_nvme_init(&a);
    cksum89_crc64_nvme_update(&a, backing, 9);
    before = a;
    cksum89_crc64_nvme_update(&a, NULL, 0);
    cksum89_test_check(memcmp(&a, &before, sizeof a) == 0,
                       "crc64 nvme null zero no-op");

    cksum89_crc64_nvme_init(&a);
    cksum89_crc64_nvme_update(&a, backing, 20);
    cksum89_crc64_nvme_init(&a);
    cksum89_crc64_nvme_update(&a, backing, 5);
    want = cksum89_crc64_nvme(backing, 5);
    cksum89_test_u64(cksum89_crc64_nvme_final(&a), want.hi, want.lo,
                     "crc64 nvme init resets");

    cksum89_crc64_nvme_init(&a);
    cksum89_crc64_nvme_update(&a, backing, 0);
    cksum89_crc64_nvme_update(&a, backing, 4);
    cksum89_crc64_nvme_update(&a, NULL, 0);
    cksum89_crc64_nvme_update(&a, backing + 4, 6);
    cksum89_crc64_nvme_update(&a, backing + 10, 0);
    want = cksum89_crc64_nvme(backing, 10);
    cksum89_test_u64(cksum89_crc64_nvme_final(&a), want.hi, want.lo,
                     "crc64 nvme zero-length inserts");
}

static void check_inet16_laws(void)
{
    cksum89_inet16_ctx a;
    cksum89_inet16_ctx b;
    cksum89_inet16_ctx before;
    cksum89_u16 first;
    cksum89_u16 second;

    cksum89_inet16_init(&a);
    cksum89_inet16_update(&a, backing, 16);
    before = a;
    first = cksum89_inet16_final(&a);
    second = cksum89_inet16_final(&a);
    cksum89_test_u16(first, second, "inet16 final repeatable");
    cksum89_test_check(memcmp(&a, &before, sizeof a) == 0,
                       "inet16 final does not mutate");

    cksum89_inet16_init(&a);
    cksum89_inet16_update(&a, backing, 5);
    b = a;
    cksum89_inet16_update(&a, backing + 5, 7);
    cksum89_inet16_update(&b, backing + 5, 3);
    cksum89_test_u16(cksum89_inet16_final(&a), cksum89_inet16(backing, 12),
                     "inet16 branch a");
    cksum89_test_u16(cksum89_inet16_final(&b), cksum89_inet16(backing, 8),
                     "inet16 branch b");

    cksum89_inet16_init(&a);
    cksum89_inet16_update(&a, backing, 9);
    before = a;
    cksum89_inet16_update(&a, NULL, 0);
    cksum89_test_check(memcmp(&a, &before, sizeof a) == 0,
                       "inet16 null zero no-op");

    cksum89_inet16_init(&a);
    cksum89_inet16_update(&a, backing, 20);
    cksum89_inet16_init(&a);
    cksum89_inet16_update(&a, backing, 5);
    cksum89_test_u16(cksum89_inet16_final(&a), cksum89_inet16(backing, 5),
                     "inet16 init resets");

    cksum89_inet16_init(&a);
    cksum89_inet16_update(&a, backing, 0);
    cksum89_inet16_update(&a, backing, 4);
    cksum89_inet16_update(&a, NULL, 0);
    cksum89_inet16_update(&a, backing + 4, 6);
    cksum89_inet16_update(&a, backing + 10, 0);
    cksum89_test_u16(cksum89_inet16_final(&a), cksum89_inet16(backing, 10),
                     "inet16 zero-length inserts");
}

static void check_alignment(void)
{
    size_t offset;

    for (offset = 0; offset <= 7; ++offset)
    {
        const unsigned char *p;
        cksum89_u64 want64;

        p = backing + offset;
        cksum89_test_u32_at(cksum89_crc32_iso_hdlc(p, 65),
                            ref_crc32_iso_hdlc(p, 65), "crc32 iso alignment",
                            (unsigned long)offset);
        cksum89_test_u32_at(cksum89_crc32c(p, 65), ref_crc32c(p, 65),
                            "crc32c alignment", (unsigned long)offset);
        want64 = ref_crc64_nvme(p, 65);
        cksum89_test_u64_at(cksum89_crc64_nvme(p, 65), want64.hi, want64.lo,
                            "crc64 nvme alignment", (unsigned long)offset);
        cksum89_test_u16_at(cksum89_inet16(p, 65), ref_inet16(p, 65),
                            "inet16 alignment", (unsigned long)offset);
    }
}

static void check_boundaries(void)
{
    static const size_t boundaries[] = {
        0,   1,   2,   3,   7,   8,    15,   16,   31,   32,   63,  64,
        127, 128, 255, 256, 257, 1023, 1024, 1025, 4095, 4096, 4097};
    size_t i;

    for (i = 0; i < (sizeof boundaries / sizeof boundaries[0]); ++i)
    {
        cksum89_u64 want64;
        size_t len;

        len = boundaries[i];
        cksum89_test_u32_at(cksum89_crc32_iso_hdlc(backing, len),
                            ref_crc32_iso_hdlc(backing, len),
                            "crc32 iso boundary", (unsigned long)len);
        cksum89_test_u32_at(cksum89_crc32c(backing, len),
                            ref_crc32c(backing, len), "crc32c boundary",
                            (unsigned long)len);
        want64 = ref_crc64_nvme(backing, len);
        cksum89_test_u64_at(cksum89_crc64_nvme(backing, len), want64.hi,
                            want64.lo, "crc64 nvme boundary",
                            (unsigned long)len);
        cksum89_test_u16_at(cksum89_inet16(backing, len),
                            ref_inet16(backing, len), "inet16 boundary",
                            (unsigned long)len);
    }
}

static void check_domains(void)
{
    cksum89_crc32c_ctx crc32c;
    cksum89_inet16_ctx inet;

    cksum89_crc32c_init(&crc32c);
    cksum89_crc32c_update(&crc32c, backing, 65);
#if ULONG_MAX > 0xffffffffUL
    cksum89_test_check((crc32c.state >> 32) == 0UL,
                       "crc32c state upper bits zero");
    cksum89_test_check((cksum89_crc32c_final(&crc32c) >> 32) == 0UL,
                       "crc32c value upper bits zero");
#endif
    cksum89_inet16_init(&inet);
    cksum89_inet16_update(&inet, backing, 65);
    cksum89_test_check(inet.sum <= 0xffffUL, "inet16 sum bounded");
    cksum89_test_check(inet.has_pending <= 1u, "inet16 pending flag bounded");
}

static void check_input_immutable(void)
{
    unsigned char copy[API_BUF + API_PAD];
    size_t i;

    fill_backing();
    for (i = 0; i < API_BUF + API_PAD; ++i)
    {
        copy[i] = backing[i];
    }
    (void)cksum89_crc32_iso_hdlc(backing, API_BUF);
    (void)cksum89_crc32c(backing, API_BUF);
    (void)cksum89_crc64_nvme(backing, API_BUF);
    (void)cksum89_inet16(backing, API_BUF);
    cksum89_test_check(memcmp(backing, copy, API_BUF + API_PAD) == 0,
                       "one-shot does not modify input");
}

void test_api(void)
{
    fill_backing();
    check_crc32_iso_laws();
    check_crc32c_laws();
    check_crc64_laws();
    check_inet16_laws();
    check_alignment();
    check_boundaries();
    check_domains();
    check_input_immutable();
}
