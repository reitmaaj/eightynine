/* test_format.c - little-endian codecs and structure encode/decode. */

#include <limits.h>
#include <string.h>

#include "test.h"

#include "ledger89_internal.h"

int main(void)
{
    unsigned char buf[LED89_SEGMENT_FOOTER_SIZE];
    unsigned char copy[LED89_SEGMENT_FOOTER_SIZE];
    led89_seg_header sh;
    led89_seg_header sh2;
    led89_rec_header rh;
    led89_rec_header rh2;
    led89_batch_header bh;
    led89_batch_header bh2;
    led89_batch_footer bf;
    led89_batch_footer bf2;
    led89_seg_footer sf;
    led89_seg_footer sf2;
    ledger89_index index;
    size_t size_out;
    size_t i;

    /* Scalar little-endian codecs. */
    led89_put_u16(buf, (led89_u16)0x0201u);
    CHECK_EQ(buf[0], 0x01u);
    CHECK_EQ(buf[1], 0x02u);
    CHECK_EQ(led89_get_u16(buf), (led89_u16)0x0201u);

    led89_put_u32(buf, 0x04030201u);
    CHECK_EQ(buf[0], 0x01u);
    CHECK_EQ(buf[3], 0x04u);
    CHECK_EQ(led89_get_u32(buf), 0x04030201u);

    led89_put_u64(buf, ((led89_u64)0x04030201u << 32) | (led89_u64)0x08070605u);
    CHECK_EQ(buf[0], 0x05u);
    CHECK_EQ(buf[7], 0x04u);
    CHECK_EQ(led89_get_u64(buf),
             ((led89_u64)0x04030201u << 32) | (led89_u64)0x08070605u);

    /* Segment header round trip and single-byte corruption. */
    sh.first_index = ((led89_u64)0x01020304u << 32) | (led89_u64)0x05060708u;
    sh.flags = 0u;
    led89_seg_header_encode(buf, &sh);
    CHECK_EQ(buf[0], (unsigned char)'L');
    CHECK_EQ(led89_seg_header_decode(buf, &sh2), LEDGER89_OK);
    CHECK_EQ(sh2.first_index, sh.first_index);
    CHECK_EQ(sh2.flags, 0u);
    for (i = 0u; i < (size_t)LED89_SEGMENT_HEADER_SIZE; ++i)
    {
        memcpy(copy, buf, (size_t)LED89_SEGMENT_HEADER_SIZE);
        copy[i] = (unsigned char)(copy[i] ^ 0xFFu);
        CHECK_EQ(led89_seg_header_decode(copy, &sh2), LEDGER89_ERR_CORRUPT);
    }

    /* Record header round trip, reserved bits, and oversized payload. */
    rh.index = (led89_u64)7u;
    rh.tag = ((led89_u64)0xDEADBEEFu << 32) | (led89_u64)0xCAFEBABEu;
    rh.payload_size = 3u;
    rh.flags = 0u;
    led89_rec_header_encode(buf, &rh);
    CHECK_EQ(led89_rec_header_decode(buf, &rh2), LEDGER89_OK);
    CHECK_EQ(rh2.index, (led89_u64)7u);
    CHECK_EQ(rh2.tag, rh.tag);
    CHECK_EQ(rh2.payload_size, 3u);

    memcpy(copy, buf, (size_t)LED89_RECORD_HEADER_SIZE);
    led89_put_u32(copy + 24, LEDGER89_MAX_RECORD_BYTES + 1u);
    CHECK_EQ(led89_rec_header_decode(copy, &rh2), LEDGER89_ERR_CORRUPT);
    memcpy(copy, buf, (size_t)LED89_RECORD_HEADER_SIZE);
    led89_put_u32(copy + 4, 1u);
    CHECK_EQ(led89_rec_header_decode(copy, &rh2), LEDGER89_ERR_CORRUPT);
    memcpy(copy, buf, (size_t)LED89_RECORD_HEADER_SIZE);
    led89_put_u32(copy + 28, 1u);
    CHECK_EQ(led89_rec_header_decode(copy, &rh2), LEDGER89_ERR_CORRUPT);
    memcpy(copy, buf, (size_t)LED89_RECORD_HEADER_SIZE);
    copy[0] = (unsigned char)'X';
    CHECK_EQ(led89_rec_header_decode(copy, &rh2), LEDGER89_ERR_CORRUPT);

    /* Batch header round trip and impossible values. */
    bh.record_count = 2u;
    bh.first_index = (led89_u64)9u;
    bh.batch_bytes = (led89_u64)LED89_MIN_BATCH_BYTES;
    led89_batch_header_encode(buf, &bh);
    CHECK_EQ(led89_batch_header_decode(buf, &bh2), LEDGER89_OK);
    CHECK_EQ(bh2.record_count, 2u);
    CHECK_EQ(bh2.first_index, (led89_u64)9u);
    CHECK_EQ(bh2.batch_bytes, bh.batch_bytes);
    memcpy(copy, buf, (size_t)LED89_BATCH_HEADER_SIZE);
    led89_put_u32(copy + 4, 0u);
    CHECK_EQ(led89_batch_header_decode(copy, &bh2), LEDGER89_ERR_CORRUPT);
    memcpy(copy, buf, (size_t)LED89_BATCH_HEADER_SIZE);
    led89_put_u64(copy + 16, (led89_u64)0u);
    CHECK_EQ(led89_batch_header_decode(copy, &bh2), LEDGER89_ERR_CORRUPT);
    memcpy(copy, buf, (size_t)LED89_BATCH_HEADER_SIZE);
    copy[0] = (unsigned char)'X';
    CHECK_EQ(led89_batch_header_decode(copy, &bh2), LEDGER89_ERR_CORRUPT);

    /* Batch footer round trip and reserved bits. */
    bf.record_count = 2u;
    bf.last_index = (led89_u64)10u;
    bf.crc = 0x11223344u;
    led89_batch_footer_encode(buf, &bf);
    CHECK_EQ(led89_batch_footer_decode(buf, &bf2), LEDGER89_OK);
    CHECK_EQ(bf2.record_count, 2u);
    CHECK_EQ(bf2.last_index, (led89_u64)10u);
    CHECK_EQ(bf2.crc, 0x11223344u);
    memcpy(copy, buf, (size_t)LED89_BATCH_FOOTER_SIZE);
    led89_put_u32(copy + 20, 1u);
    CHECK_EQ(led89_batch_footer_decode(copy, &bf2), LEDGER89_ERR_CORRUPT);
    memcpy(copy, buf, (size_t)LED89_BATCH_FOOTER_SIZE);
    copy[0] = (unsigned char)'X';
    CHECK_EQ(led89_batch_footer_decode(copy, &bf2), LEDGER89_ERR_CORRUPT);

    /* Segment footer round trip and single-byte corruption. */
    sf.last_index = (led89_u64)12u;
    sf.record_count = (led89_u64)3u;
    sf.segment_digest = 0xAABBCCDDu;
    sf.body_size = (led89_u64)100u;
    led89_seg_footer_encode(buf, &sf);
    CHECK_EQ(led89_seg_footer_decode(buf, &sf2), LEDGER89_OK);
    CHECK_EQ(sf2.last_index, (led89_u64)12u);
    CHECK_EQ(sf2.record_count, (led89_u64)3u);
    CHECK_EQ(sf2.segment_digest, 0xAABBCCDDu);
    CHECK_EQ(sf2.body_size, (led89_u64)100u);
    for (i = 0u; i < (size_t)LED89_SEGMENT_FOOTER_SIZE; ++i)
    {
        memcpy(copy, buf, (size_t)LED89_SEGMENT_FOOTER_SIZE);
        copy[i] = (unsigned char)(copy[i] ^ 0xFFu);
        CHECK_EQ(led89_seg_footer_decode(copy, &sf2), LEDGER89_ERR_CORRUPT);
    }

    /* Width conversions. */
    index = 123ul;
    CHECK_EQ(led89_u64_to_index((led89_u64)456u, &index), LEDGER89_OK);
    CHECK_EQ(index, 456ul);
#if ULONG_MAX == 0xFFFFFFFFUL
    index = 123ul;
    CHECK_EQ(led89_u64_to_index((led89_u64)0xFFFFFFFFu + 1u, &index),
             LEDGER89_ERR_RANGE);
    CHECK_EQ(index, 123ul);
#else
    index = 0ul;
    CHECK_EQ(led89_u64_to_index((led89_u64)ULONG_MAX, &index), LEDGER89_OK);
    CHECK_EQ(index, ULONG_MAX);
#endif

    size_out = 0u;
    CHECK_EQ(led89_u64_to_size((led89_u64)10u, &size_out), LEDGER89_OK);
    CHECK_EQ(size_out, 10u);

    TEST_END;
}
