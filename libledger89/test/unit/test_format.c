/* test_format.c - on-disk codecs for format version 2. */

#include <string.h>

#include "test.h"

#include "ledger89_internal.h"

static void test_scalars(void)
{
    unsigned char buf[8];

    led89_put_u16(buf, 0x1234u);
    CHECK_EQ(buf[0], 0x34u);
    CHECK_EQ(buf[1], 0x12u);
    CHECK_EQ(led89_get_u16(buf), 0x1234u);

    led89_put_u32(buf, 0x89ABCDEFu);
    CHECK_EQ(led89_get_u32(buf), 0x89ABCDEFu);

    led89_put_u64(buf, ((led89_u64)0x11223344u << 32) | (led89_u64)0x55667788u);
    CHECK_EQ(led89_get_u64(buf),
             ((led89_u64)0x11223344u << 32) | (led89_u64)0x55667788u);
}

static void test_current(void)
{
    led89_current c;
    led89_current out;
    unsigned char buf[LED89_CURRENT_SIZE];

    c.generation = (led89_u64)9;
    led89_current_encode(buf, &c);
    CHECK_EQ(led89_current_decode(buf, &out), LEDGER89_OK);
    CHECK_EQ(out.generation, (led89_u64)9);
    buf[0] = (unsigned char)'X';
    CHECK_EQ(led89_current_decode(buf, &out), LEDGER89_ECORRUPT);
    led89_current_encode(buf, &c);
    buf[20] ^= 0x01u;
    CHECK_EQ(led89_current_decode(buf, &out), LEDGER89_ECORRUPT);
    led89_current_encode(buf, &c);
    buf[16] = 3u;
    CHECK_EQ(led89_current_decode(buf, &out), LEDGER89_EFORMAT);
}

static void test_part_header(void)
{
    led89_part_header h;
    led89_part_header out;
    unsigned char buf[LED89_PART_HEADER_SIZE];

    memset(&h, 0, sizeof h);
    h.uuid[0] = 0xABu;
    h.file_id = (led89_u64)7;
    h.revision = (led89_u64)3;
    h.first = (led89_u64)100;
    led89_part_header_encode(buf, &h);
    CHECK_EQ(led89_part_header_decode(buf, &out), LEDGER89_OK);
    CHECK_EQ(out.file_id, (led89_u64)7);
    CHECK_EQ(out.revision, (led89_u64)3);
    CHECK_EQ(out.first, (led89_u64)100);
    CHECK_EQ(out.uuid[0], 0xABu);
    buf[0] = 0u;
    CHECK_EQ(led89_part_header_decode(buf, &out), LEDGER89_ECORRUPT);
}

static void test_batch(void)
{
    led89_batch_header h;
    led89_batch_header hout;
    led89_batch_footer f;
    led89_batch_footer fout;
    unsigned char hb[LED89_BATCH_HEADER_SIZE];
    unsigned char fb[LED89_BATCH_FOOTER_SIZE];

    h.count = 2u;
    h.first = (led89_u64)10;
    h.bytes = (led89_u64)LED89_MIN_BATCH_BYTES + (led89_u64)8;
    led89_batch_header_encode(hb, &h);
    CHECK_EQ(led89_is_batch_header(hb), 1);
    CHECK_EQ(led89_batch_header_decode(hb, &hout), LEDGER89_OK);
    CHECK_EQ(hout.count, 2u);
    CHECK_EQ(hout.first, (led89_u64)10);
    CHECK_EQ(hout.bytes, h.bytes);
    hb[0] ^= 0xFFu;
    CHECK_EQ(led89_batch_header_decode(hb, &hout), LEDGER89_ECORRUPT);

    f.count = 2u;
    f.last = (led89_u64)11;
    led89_batch_footer_encode(fb, &f);
    CHECK_EQ(led89_is_batch_footer(fb), 1);
    CHECK_EQ(led89_batch_footer_decode(fb, &fout), LEDGER89_OK);
    CHECK_EQ(fout.count, 2u);
    CHECK_EQ(fout.last, (led89_u64)11);
}

static void test_marker(void)
{
    led89_marker m;
    led89_marker out;
    unsigned char buf[LED89_MARKER_SIZE];

    memset(&m, 0, sizeof m);
    m.uuid[0] = 1u;
    m.file_id = (led89_u64)2;
    m.revision = (led89_u64)5;
    m.end = (led89_u64)42;
    led89_marker_encode(buf, &m);
    CHECK_EQ(led89_is_marker(buf), 1);
    CHECK_EQ(led89_marker_decode(buf, &out), LEDGER89_OK);
    CHECK_EQ(out.file_id, (led89_u64)2);
    CHECK_EQ(out.revision, (led89_u64)5);
    CHECK_EQ(out.end, (led89_u64)42);
    buf[10] ^= 0x80u;
    CHECK_EQ(led89_marker_decode(buf, &out), LEDGER89_ECORRUPT);
}

static void test_sealed_footer(void)
{
    led89_sealed_footer f;
    led89_sealed_footer out;
    unsigned char buf[LED89_SEALED_FOOTER_SIZE];

    memset(&f, 0, sizeof f);
    f.uuid[3] = 9u;
    f.file_id = (led89_u64)4;
    f.first = (led89_u64)1;
    f.end = (led89_u64)11;
    f.records = (led89_u64)10;
    f.digest = 0xDEADBEEFu;
    led89_sealed_footer_encode(buf, &f);
    CHECK_EQ(led89_sealed_footer_decode(buf, &out), LEDGER89_OK);
    CHECK_EQ(out.file_id, (led89_u64)4);
    CHECK_EQ(out.end, (led89_u64)11);
    CHECK_EQ(out.records, (led89_u64)10);
    CHECK_EQ(out.digest, 0xDEADBEEFu);
    buf[60] ^= 0x01u;
    CHECK_EQ(led89_sealed_footer_decode(buf, &out), LEDGER89_ECORRUPT);
}

static void test_manifest(void)
{
    led89_manifest m;
    led89_manifest out;
    led89_part_desc sealed[2];
    unsigned char *buf;
    size_t size;

    memset(&m, 0, sizeof m);
    m.generation = (led89_u64)3;
    m.uuid[0] = 0x5Au;
    m.revision = (led89_u64)2;
    m.first = (led89_u64)1;
    sealed[0].file_id = (led89_u64)1;
    sealed[0].first = (led89_u64)1;
    sealed[0].end = (led89_u64)10;
    sealed[1].file_id = (led89_u64)2;
    sealed[1].first = (led89_u64)10;
    sealed[1].end = (led89_u64)20;
    m.sealed_count = 2u;
    m.sealed = sealed;
    m.active.file_id = (led89_u64)3;
    m.active.first = (led89_u64)20;
    m.active.end = (led89_u64)20;

    size = led89_manifest_bytes(2u);
    buf = (unsigned char *)malloc(size);
    CHECK(buf != NULL);
    if (buf == NULL)
    {
        return;
    }
    led89_manifest_encode(buf, &m);
    CHECK_EQ(led89_manifest_decode(buf, size, &out), LEDGER89_OK);
    CHECK_EQ(out.generation, (led89_u64)3);
    CHECK_EQ(out.revision, (led89_u64)2);
    CHECK_EQ(out.first, (led89_u64)1);
    CHECK_EQ(out.sealed_count, 2u);
    CHECK_EQ(out.sealed[1].file_id, (led89_u64)2);
    CHECK_EQ(out.active.file_id, (led89_u64)3);
    CHECK_EQ(out.active.first, (led89_u64)20);
    led89_manifest_free(&out);

    CHECK_EQ(led89_manifest_decode(buf, size - 1u, &out), LEDGER89_ECORRUPT);

    /* Break contiguity: sealed[1].first must equal sealed[0].end. */
    led89_put_u64(buf + LED89_MANIFEST_HEADER_SIZE +
                      (size_t)LED89_SEALED_DESC_SIZE + 8u,
                  (led89_u64)11);
    led89_put_u32(buf + size - 4u, led89_crc32c(0u, buf, size - 4u));
    CHECK_EQ(led89_manifest_decode(buf, size, &out), LEDGER89_ECORRUPT);

    free(buf);
}

int main(void)
{
    test_scalars();
    test_current();
    test_part_header();
    test_batch();
    test_marker();
    test_sealed_footer();
    test_manifest();
    TEST_END;
}
