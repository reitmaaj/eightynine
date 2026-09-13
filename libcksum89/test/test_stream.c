/* test_stream.c - streaming algebra: every partition of a byte string
 * equals the one-shot value. Exhaustive over all chunkings of a 12-byte
 * string, plus a byte-at-a-time pass. */

#include "test.h"

#define STREAM_MAX 12

static const unsigned char stream_data[STREAM_MAX] = {
    0x00, 0x01, 0xfe, 0xff, 0x7f, 0x80, 0x55, 0xaa, 0x12, 0x34, 0x56, 0x78};

static void check_iso_partition(unsigned int mask)
{
    cksum89_crc32_iso_hdlc_ctx ctx;
    cksum89_u32 one_shot;
    size_t start;
    size_t i;

    one_shot = cksum89_crc32_iso_hdlc(stream_data, STREAM_MAX);
    cksum89_crc32_iso_hdlc_init(&ctx);
    start = 0;
    for (i = 1; i < STREAM_MAX; ++i)
    {
        if ((mask & (1u << (i - 1u))) != 0u)
        {
            cksum89_crc32_iso_hdlc_update(&ctx, stream_data + start, i - start);
            start = i;
        }
    }
    cksum89_crc32_iso_hdlc_update(&ctx, stream_data + start,
                                  STREAM_MAX - start);
    cksum89_test_u32_at(cksum89_crc32_iso_hdlc_final(&ctx), one_shot,
                        "crc32 iso partition", (unsigned long)mask);
}

static void check_crc32c_partition(unsigned int mask)
{
    cksum89_crc32c_ctx ctx;
    cksum89_u32 one_shot;
    size_t start;
    size_t i;

    one_shot = cksum89_crc32c(stream_data, STREAM_MAX);
    cksum89_crc32c_init(&ctx);
    start = 0;
    for (i = 1; i < STREAM_MAX; ++i)
    {
        if ((mask & (1u << (i - 1u))) != 0u)
        {
            cksum89_crc32c_update(&ctx, stream_data + start, i - start);
            start = i;
        }
    }
    cksum89_crc32c_update(&ctx, stream_data + start, STREAM_MAX - start);
    cksum89_test_u32_at(cksum89_crc32c_final(&ctx), one_shot,
                        "crc32c partition", (unsigned long)mask);
}

static void check_crc32c_byte_at_a_time(void)
{
    cksum89_crc32c_ctx ctx;
    cksum89_u32 one_shot;
    size_t i;

    one_shot = cksum89_crc32c(stream_data, STREAM_MAX);
    cksum89_crc32c_init(&ctx);
    for (i = 0; i < STREAM_MAX; ++i)
    {
        cksum89_crc32c_update(&ctx, stream_data + i, 1);
    }
    cksum89_test_u32(cksum89_crc32c_final(&ctx), one_shot,
                     "crc32c byte-at-a-time");
}

static void check_iso_byte_at_a_time(void)
{
    cksum89_crc32_iso_hdlc_ctx ctx;
    cksum89_u32 one_shot;
    size_t i;

    one_shot = cksum89_crc32_iso_hdlc(stream_data, STREAM_MAX);
    cksum89_crc32_iso_hdlc_init(&ctx);
    for (i = 0; i < STREAM_MAX; ++i)
    {
        cksum89_crc32_iso_hdlc_update(&ctx, stream_data + i, 1);
    }
    cksum89_test_u32(cksum89_crc32_iso_hdlc_final(&ctx), one_shot,
                     "crc32 iso byte-at-a-time");
}

void test_stream(void)
{
    unsigned int mask;

    for (mask = 0; mask < (1u << (STREAM_MAX - 1)); ++mask)
    {
        check_iso_partition(mask);
        check_crc32c_partition(mask);
    }
    check_iso_byte_at_a_time();
    check_crc32c_byte_at_a_time();
}
