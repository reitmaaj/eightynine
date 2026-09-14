/* test_codec_tables.c - table-driven white-box coverage for the
 * little-endian scalar codecs and every on-disk structure codec. */

#include <string.h>

#include "test.h"

#include "ledger89_internal.h"

enum
{
    MUT_NONE = 0,
    MUT_MAGIC,
    MUT_VERSION,
    MUT_CRC
};

enum
{
    MM_NONE = 0,
    MM_MAGIC,
    MM_VERSION,
    MM_HDR_CRC,
    MM_FINAL_CRC,
    MM_COUNT,
    MM_CONTIG,
    MM_ACTIVE_FIRST,
    MM_DESC_END,
    MM_DESC_FIRST
};

struct u16_case
{
    led89_u16 v;
    unsigned char b[2];
};

static const struct u16_case u16_cases[] = {
    {0u, {0x00u, 0x00u}},      {1u, {0x01u, 0x00u}},
    {0xFFu, {0xFFu, 0x00u}},   {0x0100u, {0x00u, 0x01u}},
    {0x1234u, {0x34u, 0x12u}}, {0xFFFFu, {0xFFu, 0xFFu}}};

struct u32_case
{
    led89_u32 v;
    unsigned char b[4];
};

static const struct u32_case u32_cases[] = {
    {0u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {1u, {0x01u, 0x00u, 0x00u, 0x00u}},
    {0xFFu, {0xFFu, 0x00u, 0x00u, 0x00u}},
    {0x0100u, {0x00u, 0x01u, 0x00u, 0x00u}},
    {0x12345678u, {0x78u, 0x56u, 0x34u, 0x12u}},
    {0x89ABCDEFu, {0xEFu, 0xCDu, 0xABu, 0x89u}},
    {0xFFFFFFFFu, {0xFFu, 0xFFu, 0xFFu, 0xFFu}}};

struct u64_case
{
    led89_u64 v;
    unsigned char b[8];
};

static const struct u64_case u64_cases[] = {
    {(led89_u64)0, {0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u}},
    {(led89_u64)1, {0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u}},
    {(led89_u64)0xFFFFFFFFu,
     {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0x00u, 0x00u, 0x00u, 0x00u}},
    {((led89_u64)1 << 32),
     {0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u}},
    {((led89_u64)0x11223344u << 32) | (led89_u64)0x55667788u,
     {0x88u, 0x77u, 0x66u, 0x55u, 0x44u, 0x33u, 0x22u, 0x11u}},
    {~((led89_u64)0),
     {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu}}};

struct current_case
{
    led89_u64 generation;
    int mutation;
    int expected;
};

static const struct current_case current_cases[] = {
    {(led89_u64)0, MUT_NONE, LEDGER89_OK},
    {(led89_u64)1, MUT_NONE, LEDGER89_OK},
    {~((led89_u64)0), MUT_NONE, LEDGER89_OK},
    {(led89_u64)9, MUT_MAGIC, LEDGER89_ECORRUPT},
    {(led89_u64)9, MUT_VERSION, LEDGER89_EFORMAT},
    {(led89_u64)9, MUT_CRC, LEDGER89_ECORRUPT}};

static const led89_u32 manifest_counts[] = {0u, 1u, 2u, 3u, 10u};

static const led89_u64 manifest_size_values[] = {84u, 108u, 132u, 156u, 324u};

struct manifest_seal_case
{
    led89_u64 file_id;
    led89_u64 first;
    led89_u64 end;
};

static const struct manifest_seal_case manifest_seals[3] = {
    {(led89_u64)1, (led89_u64)1, (led89_u64)10},
    {(led89_u64)2, (led89_u64)10, (led89_u64)20},
    {(led89_u64)3, (led89_u64)20, (led89_u64)30}};

struct manifest_reject_case
{
    int mutation;
    led89_u32 count;
    int size_delta;
    int expected;
};

static const struct manifest_reject_case manifest_reject_cases[] = {
    {MM_MAGIC, 2u, 0, LEDGER89_ECORRUPT},
    {MM_VERSION, 2u, 0, LEDGER89_EFORMAT},
    {MM_HDR_CRC, 2u, 0, LEDGER89_ECORRUPT},
    {MM_FINAL_CRC, 2u, 0, LEDGER89_ECORRUPT},
    {MM_COUNT, 2u, 0, LEDGER89_ECORRUPT},
    {MM_CONTIG, 2u, 0, LEDGER89_ECORRUPT},
    {MM_ACTIVE_FIRST, 2u, 0, LEDGER89_ECORRUPT},
    {MM_DESC_END, 1u, 0, LEDGER89_ECORRUPT},
    {MM_DESC_FIRST, 1u, 0, LEDGER89_ECORRUPT},
    {MM_NONE, 2u, -1, LEDGER89_ECORRUPT},
    {MM_NONE, 2u, 1, LEDGER89_ECORRUPT},
    {MM_NONE, 2u, -100, LEDGER89_ECORRUPT}};

struct part_header_case
{
    unsigned char uuid0;
    led89_u64 file_id;
    led89_u64 revision;
    led89_u64 first;
};

static const struct part_header_case part_header_cases[] = {
    {0x00u, (led89_u64)0, (led89_u64)0, (led89_u64)0},
    {0xABu, (led89_u64)7, (led89_u64)3, (led89_u64)100},
    {0xFFu, ~((led89_u64)0), ~((led89_u64)0), (led89_u64)1}};

struct batch_header_case
{
    led89_u32 count;
    led89_u64 first;
    led89_u64 bytes;
    int mutation;
    int expected;
};

static const struct batch_header_case batch_header_cases[] = {
    {1u, (led89_u64)1, (led89_u64)LED89_MIN_BATCH_BYTES, MUT_NONE, LEDGER89_OK},
    {2u, (led89_u64)10, (led89_u64)LED89_MIN_BATCH_BYTES + (led89_u64)8,
     MUT_NONE, LEDGER89_OK},
    {0xFFFFFFFFu, (led89_u64)0, (led89_u64)LED89_MIN_BATCH_BYTES, MUT_NONE,
     LEDGER89_OK},
    {0u, (led89_u64)1, (led89_u64)LED89_MIN_BATCH_BYTES, MUT_NONE,
     LEDGER89_ECORRUPT},
    {1u, (led89_u64)1, (led89_u64)(LED89_MIN_BATCH_BYTES - 1u), MUT_NONE,
     LEDGER89_ECORRUPT},
    {1u, (led89_u64)1, (led89_u64)0, MUT_NONE, LEDGER89_ECORRUPT},
    {1u, (led89_u64)1, (led89_u64)LED89_MIN_BATCH_BYTES, MUT_MAGIC,
     LEDGER89_ECORRUPT},
    {1u, (led89_u64)1, (led89_u64)LED89_MIN_BATCH_BYTES, MUT_CRC,
     LEDGER89_ECORRUPT}};

struct batch_footer_case
{
    led89_u32 count;
    led89_u64 last;
    int mutation;
    int expected;
};

static const struct batch_footer_case batch_footer_cases[] = {
    {0u, (led89_u64)0, MUT_NONE, LEDGER89_OK},
    {2u, (led89_u64)11, MUT_NONE, LEDGER89_OK},
    {0xFFFFFFFFu, ~((led89_u64)0), MUT_NONE, LEDGER89_OK},
    {2u, (led89_u64)11, MUT_MAGIC, LEDGER89_ECORRUPT}};

struct marker_case
{
    led89_u64 file_id;
    led89_u64 revision;
    led89_u64 end;
    int mutation;
    int expected;
};

static const struct marker_case marker_cases[] = {
    {(led89_u64)0, (led89_u64)0, (led89_u64)0, MUT_NONE, LEDGER89_OK},
    {(led89_u64)2, (led89_u64)5, (led89_u64)42, MUT_NONE, LEDGER89_OK},
    {~((led89_u64)0), ~((led89_u64)0), ~((led89_u64)0), MUT_NONE, LEDGER89_OK},
    {(led89_u64)2, (led89_u64)5, (led89_u64)42, MUT_MAGIC, LEDGER89_ECORRUPT},
    {(led89_u64)2, (led89_u64)5, (led89_u64)42, MUT_VERSION, LEDGER89_EFORMAT},
    {(led89_u64)2, (led89_u64)5, (led89_u64)42, MUT_CRC, LEDGER89_ECORRUPT}};

struct sealed_footer_case
{
    led89_u64 file_id;
    led89_u64 first;
    led89_u64 end;
    led89_u64 records;
    led89_u32 digest;
    int mutation;
    int expected;
};

static const struct sealed_footer_case sealed_footer_cases[] = {
    {(led89_u64)0, (led89_u64)0, (led89_u64)0, (led89_u64)0, 0u, MUT_NONE,
     LEDGER89_OK},
    {(led89_u64)4, (led89_u64)1, (led89_u64)11, (led89_u64)10, 0xDEADBEEFu,
     MUT_NONE, LEDGER89_OK},
    {~((led89_u64)0), ~((led89_u64)0), ~((led89_u64)0), ~((led89_u64)0),
     0xFFFFFFFFu, MUT_NONE, LEDGER89_OK},
    {(led89_u64)4, (led89_u64)1, (led89_u64)11, (led89_u64)10, 0xDEADBEEFu,
     MUT_MAGIC, LEDGER89_ECORRUPT},
    {(led89_u64)4, (led89_u64)1, (led89_u64)11, (led89_u64)10, 0xDEADBEEFu,
     MUT_CRC, LEDGER89_ECORRUPT}};

struct magic_case
{
    unsigned char bytes[8];
    int is_header;
    int is_footer;
    int is_marker;
};

static const struct magic_case magic_cases[] = {
    {{'B', '8', '9', 0x02u, 0u, 0u, 0u, 0u}, 1, 0, 0},
    {{'b', '8', '9', 0x02u, 0u, 0u, 0u, 0u}, 0, 1, 0},
    {{'B', '8', '9', 0x03u, 0u, 0u, 0u, 0u}, 0, 0, 0},
    {{'L', 'D', '8', '9', 'S', 'T', 'B', '2'}, 0, 0, 1},
    {{'L', 'D', '8', '9', 'S', 'T', 'B', '3'}, 0, 0, 0},
    {{'L', 'D', '8', '9', 'S', 'T', 'B', 0x00u}, 0, 0, 0},
    {{0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}, 0, 0, 0}};

struct bytes_case
{
    unsigned char a[4];
    unsigned char b[4];
    size_t n;
    int expected;
};

static const struct bytes_case bytes_cases[] = {
    {{1u, 2u, 3u, 4u}, {1u, 2u, 3u, 4u}, 4u, 1},
    {{1u, 2u, 3u, 4u}, {9u, 2u, 3u, 4u}, 4u, 0},
    {{1u, 2u, 3u, 4u}, {1u, 2u, 3u, 9u}, 4u, 0},
    {{1u, 2u, 3u, 4u}, {1u, 2u, 9u, 4u}, 2u, 1},
    {{1u, 2u, 3u, 4u}, {9u, 2u, 3u, 4u}, 0u, 1},
    {{0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u}, 4u, 1}};

static void test_scalars(void)
{
    unsigned char buf[8];
    size_t i;

    for (i = 0u; i < sizeof u16_cases / sizeof u16_cases[0]; ++i)
    {
        led89_put_u16(buf, u16_cases[i].v);
        CHECK(memcmp(buf, u16_cases[i].b, 2u) == 0);
        CHECK_EQ(led89_get_u16(buf), u16_cases[i].v);
    }
    for (i = 0u; i < sizeof u32_cases / sizeof u32_cases[0]; ++i)
    {
        led89_put_u32(buf, u32_cases[i].v);
        CHECK(memcmp(buf, u32_cases[i].b, 4u) == 0);
        CHECK_EQ(led89_get_u32(buf), u32_cases[i].v);
    }
    for (i = 0u; i < sizeof u64_cases / sizeof u64_cases[0]; ++i)
    {
        led89_put_u64(buf, u64_cases[i].v);
        CHECK(memcmp(buf, u64_cases[i].b, 8u) == 0);
        CHECK(led89_get_u64(buf) == u64_cases[i].v);
    }
}

static void test_current(void)
{
    unsigned char buf[LED89_CURRENT_SIZE];
    led89_current seed;
    size_t i;

    seed.generation = ((led89_u64)0x11223344u << 32) | (led89_u64)0x55667788u;
    led89_current_encode(buf, &seed);
    CHECK(memcmp(buf, "LD89CUR2", 8u) == 0);
    CHECK(led89_get_u64(buf + 8u) == seed.generation);
    CHECK_EQ(led89_get_u32(buf + 16u), 2u);
    CHECK_EQ(buf[24], 0u);
    CHECK_EQ(buf[31], 0u);

    for (i = 0u; i < sizeof current_cases / sizeof current_cases[0]; ++i)
    {
        const struct current_case *c;
        led89_current in;
        led89_current out;
        int rc;

        c = &current_cases[i];
        in.generation = c->generation;
        led89_current_encode(buf, &in);
        if (c->mutation == MUT_MAGIC)
        {
            buf[0] = (unsigned char)'X';
        }
        if (c->mutation == MUT_VERSION)
        {
            buf[16] = 3u;
        }
        if (c->mutation == MUT_CRC)
        {
            buf[20] ^= 0x01u;
        }
        out.generation = (led89_u64)0;
        rc = led89_current_decode(buf, &out);
        CHECK_EQ(rc, c->expected);
        if (rc == LEDGER89_OK)
        {
            CHECK(out.generation == c->generation);
        }
    }
}

static void manifest_fill(led89_manifest *m, led89_part_desc *sealed,
                          led89_u32 count)
{
    led89_u32 i;

    m->generation = (led89_u64)3;
    memset(m->uuid, 0, 16u);
    m->uuid[0] = 0x5Au;
    m->revision = (led89_u64)2;
    m->first = (led89_u64)1;
    m->sealed_count = count;
    m->sealed = sealed;
    m->active.file_id = (led89_u64)10 + (led89_u64)count;
    m->active.first = (led89_u64)1;
    for (i = 0u; i < count; ++i)
    {
        m->active.first = sealed[i].end;
    }
    m->active.end = m->active.first;
}

static void manifest_mutate(unsigned char *buf, size_t size, led89_u32 count,
                            int mutation)
{
    size_t desc0;
    size_t active;

    desc0 = (size_t)LED89_MANIFEST_HEADER_SIZE;
    active = desc0 + ((size_t)count * (size_t)LED89_SEALED_DESC_SIZE);
    switch (mutation)
    {
    case MM_MAGIC:
        buf[0] = (unsigned char)'X';
        break;
    case MM_VERSION:
        led89_put_u32(buf + 52u, 3u);
        break;
    case MM_HDR_CRC:
        buf[56] ^= 0x01u;
        break;
    case MM_FINAL_CRC:
        buf[size - 4u] ^= 0x01u;
        break;
    case MM_COUNT:
        led89_put_u32(buf + 48u, count + 1u);
        led89_put_u32(buf + 56u, led89_crc32c(0u, buf, 56u));
        break;
    case MM_CONTIG:
        led89_put_u64(buf + desc0 + (size_t)LED89_SEALED_DESC_SIZE + 8u,
                      (led89_u64)999);
        led89_put_u32(buf + size - 4u, led89_crc32c(0u, buf, size - 4u));
        break;
    case MM_ACTIVE_FIRST:
        led89_put_u64(buf + active + 8u, (led89_u64)999);
        led89_put_u32(buf + size - 4u, led89_crc32c(0u, buf, size - 4u));
        break;
    case MM_DESC_END:
        led89_put_u64(buf + desc0 + 16u, (led89_u64)0);
        led89_put_u32(buf + size - 4u, led89_crc32c(0u, buf, size - 4u));
        break;
    case MM_DESC_FIRST:
        led89_put_u64(buf + desc0 + 8u, (led89_u64)2);
        led89_put_u32(buf + size - 4u, led89_crc32c(0u, buf, size - 4u));
        break;
    default:
        break;
    }
}

static void test_manifest_sizes(void)
{
    size_t i;

    for (i = 0u; i < sizeof manifest_counts / sizeof manifest_counts[0]; ++i)
    {
        CHECK_EQ(led89_manifest_bytes(manifest_counts[i]),
                 (size_t)manifest_size_values[i]);
    }
}

static void test_manifest_roundtrip(void)
{
    size_t i;

    for (i = 0u; i < sizeof manifest_counts / sizeof manifest_counts[0]; ++i)
    {
        led89_u32 count;
        led89_part_desc descs[3];
        led89_manifest m;
        led89_manifest out;
        unsigned char *buf;
        size_t size;
        led89_u32 j;

        count = manifest_counts[i];
        if (count > 3u)
        {
            continue;
        }
        for (j = 0u; j < count; ++j)
        {
            descs[j].file_id = manifest_seals[j].file_id;
            descs[j].first = manifest_seals[j].first;
            descs[j].end = manifest_seals[j].end;
        }
        manifest_fill(&m, descs, count);
        size = led89_manifest_bytes(count);
        buf = (unsigned char *)malloc(size);
        CHECK(buf != NULL);
        if (buf == NULL)
        {
            continue;
        }
        led89_manifest_encode(buf, &m);
        CHECK(memcmp(buf, "LD89MAN2", 8u) == 0);
        CHECK_EQ(led89_get_u32(buf + 52u), 2u);
        CHECK_EQ(led89_get_u32(buf + 48u), count);
        CHECK_EQ(led89_manifest_decode(buf, size, &out), LEDGER89_OK);
        CHECK(out.generation == m.generation);
        CHECK(out.revision == m.revision);
        CHECK(out.first == m.first);
        CHECK_EQ(out.sealed_count, count);
        CHECK(out.active.file_id == m.active.file_id);
        CHECK(out.active.first == m.active.first);
        for (j = 0u; j < count; ++j)
        {
            CHECK(out.sealed[j].file_id == descs[j].file_id);
            CHECK(out.sealed[j].first == descs[j].first);
            CHECK(out.sealed[j].end == descs[j].end);
        }
        led89_manifest_free(&out);
        free(buf);
    }
}

static void test_manifest_rejections(void)
{
    size_t i;

    for (i = 0u;
         i < sizeof manifest_reject_cases / sizeof manifest_reject_cases[0];
         ++i)
    {
        const struct manifest_reject_case *c;
        led89_part_desc descs[3];
        led89_manifest m;
        led89_manifest out;
        unsigned char *buf;
        size_t size;
        size_t decode_size;
        led89_u32 j;
        int rc;

        c = &manifest_reject_cases[i];
        for (j = 0u; j < c->count; ++j)
        {
            descs[j].file_id = manifest_seals[j].file_id;
            descs[j].first = manifest_seals[j].first;
            descs[j].end = manifest_seals[j].end;
        }
        manifest_fill(&m, descs, c->count);
        size = led89_manifest_bytes(c->count);
        buf = (unsigned char *)malloc(size + 8u);
        CHECK(buf != NULL);
        if (buf == NULL)
        {
            continue;
        }
        led89_manifest_encode(buf, &m);
        manifest_mutate(buf, size, c->count, c->mutation);
        decode_size = size;
        if (c->size_delta < 0)
        {
            decode_size = size - (size_t)(-c->size_delta);
        }
        if (c->size_delta > 0)
        {
            decode_size = size + (size_t)c->size_delta;
        }
        out.sealed = NULL;
        rc = led89_manifest_decode(buf, decode_size, &out);
        CHECK_EQ(rc, c->expected);
        if (rc == LEDGER89_OK)
        {
            led89_manifest_free(&out);
        }
        free(buf);
    }
}

static void test_part_header(void)
{
    unsigned char buf[LED89_PART_HEADER_SIZE];
    size_t i;

    for (i = 0u; i < sizeof part_header_cases / sizeof part_header_cases[0];
         ++i)
    {
        const struct part_header_case *c;
        led89_part_header h;
        led89_part_header out;
        int rc;

        c = &part_header_cases[i];
        memset(&h, 0, sizeof h);
        h.uuid[0] = c->uuid0;
        h.file_id = c->file_id;
        h.revision = c->revision;
        h.first = c->first;
        led89_part_header_encode(buf, &h);
        CHECK_EQ(led89_get_u32(buf + 48u), 2u);
        rc = led89_part_header_decode(buf, &out);
        CHECK_EQ(rc, LEDGER89_OK);
        CHECK_EQ(out.uuid[0], c->uuid0);
        CHECK(out.file_id == c->file_id);
        CHECK(out.revision == c->revision);
        CHECK(out.first == c->first);
    }
}

static void test_part_header_rejections(void)
{
    led89_part_header h;
    led89_part_header out;
    unsigned char buf[LED89_PART_HEADER_SIZE];

    memset(&h, 0, sizeof h);
    led89_part_header_encode(buf, &h);
    buf[0] ^= 0x01u;
    CHECK_EQ(led89_part_header_decode(buf, &out), LEDGER89_ECORRUPT);

    led89_part_header_encode(buf, &h);
    buf[48] = 3u;
    CHECK_EQ(led89_part_header_decode(buf, &out), LEDGER89_EFORMAT);

    led89_part_header_encode(buf, &h);
    buf[56] ^= 0x01u;
    CHECK_EQ(led89_part_header_decode(buf, &out), LEDGER89_ECORRUPT);
}

static void test_batch_header(void)
{
    unsigned char buf[LED89_BATCH_HEADER_SIZE];
    size_t i;

    for (i = 0u; i < sizeof batch_header_cases / sizeof batch_header_cases[0];
         ++i)
    {
        const struct batch_header_case *c;
        led89_batch_header h;
        led89_batch_header out;
        int rc;

        c = &batch_header_cases[i];
        h.count = c->count;
        h.first = c->first;
        h.bytes = c->bytes;
        led89_batch_header_encode(buf, &h);
        CHECK_EQ(led89_is_batch_header(buf), 1);
        if (c->mutation == MUT_MAGIC)
        {
            buf[0] ^= 0xFFu;
        }
        if (c->mutation == MUT_CRC)
        {
            buf[24] ^= 0x01u;
        }
        out.count = 0u;
        out.first = (led89_u64)0;
        out.bytes = (led89_u64)0;
        rc = led89_batch_header_decode(buf, &out);
        CHECK_EQ(rc, c->expected);
        if (rc == LEDGER89_OK)
        {
            CHECK_EQ(out.count, c->count);
            CHECK(out.first == c->first);
            CHECK(out.bytes == c->bytes);
        }
    }
}

static void test_batch_footer(void)
{
    unsigned char buf[LED89_BATCH_FOOTER_SIZE];
    size_t i;

    for (i = 0u; i < sizeof batch_footer_cases / sizeof batch_footer_cases[0];
         ++i)
    {
        const struct batch_footer_case *c;
        led89_batch_footer f;
        led89_batch_footer out;
        int rc;

        c = &batch_footer_cases[i];
        f.count = c->count;
        f.last = c->last;
        led89_batch_footer_encode(buf, &f);
        CHECK_EQ(led89_is_batch_footer(buf), 1);
        if (c->mutation == MUT_MAGIC)
        {
            buf[0] ^= 0xFFu;
        }
        out.count = 0u;
        out.last = (led89_u64)0;
        rc = led89_batch_footer_decode(buf, &out);
        CHECK_EQ(rc, c->expected);
        if (rc == LEDGER89_OK)
        {
            CHECK_EQ(out.count, c->count);
            CHECK(out.last == c->last);
        }
    }
}

static void test_marker(void)
{
    unsigned char buf[LED89_MARKER_SIZE];
    size_t i;

    for (i = 0u; i < sizeof marker_cases / sizeof marker_cases[0]; ++i)
    {
        const struct marker_case *c;
        led89_marker m;
        led89_marker out;
        int rc;

        c = &marker_cases[i];
        memset(&m, 0, sizeof m);
        m.file_id = c->file_id;
        m.revision = c->revision;
        m.end = c->end;
        led89_marker_encode(buf, &m);
        CHECK_EQ(led89_is_marker(buf), 1);
        CHECK(memcmp(buf + 56u, "89STABLE", 8u) == 0);
        if (c->mutation == MUT_MAGIC)
        {
            buf[0] ^= 0x01u;
        }
        if (c->mutation == MUT_VERSION)
        {
            buf[48] = 3u;
        }
        if (c->mutation == MUT_CRC)
        {
            buf[52] ^= 0x01u;
        }
        out.file_id = (led89_u64)0;
        out.revision = (led89_u64)0;
        out.end = (led89_u64)0;
        rc = led89_marker_decode(buf, &out);
        CHECK_EQ(rc, c->expected);
        if (rc == LEDGER89_OK)
        {
            CHECK(out.file_id == c->file_id);
            CHECK(out.revision == c->revision);
            CHECK(out.end == c->end);
        }
    }
}

static void test_sealed_footer(void)
{
    unsigned char buf[LED89_SEALED_FOOTER_SIZE];
    size_t i;

    for (i = 0u; i < sizeof sealed_footer_cases / sizeof sealed_footer_cases[0];
         ++i)
    {
        const struct sealed_footer_case *c;
        led89_sealed_footer f;
        led89_sealed_footer out;
        int rc;

        c = &sealed_footer_cases[i];
        memset(&f, 0, sizeof f);
        f.file_id = c->file_id;
        f.first = c->first;
        f.end = c->end;
        f.records = c->records;
        f.digest = c->digest;
        led89_sealed_footer_encode(buf, &f);
        if (c->mutation == MUT_MAGIC)
        {
            buf[0] ^= 0x01u;
        }
        if (c->mutation == MUT_CRC)
        {
            buf[60] ^= 0x01u;
        }
        out.file_id = (led89_u64)0;
        out.first = (led89_u64)0;
        out.end = (led89_u64)0;
        out.records = (led89_u64)0;
        out.digest = 0u;
        rc = led89_sealed_footer_decode(buf, &out);
        CHECK_EQ(rc, c->expected);
        if (rc == LEDGER89_OK)
        {
            CHECK(out.file_id == c->file_id);
            CHECK(out.first == c->first);
            CHECK(out.end == c->end);
            CHECK(out.records == c->records);
            CHECK_EQ(out.digest, c->digest);
        }
    }
}

static void test_magic_predicates(void)
{
    size_t i;

    for (i = 0u; i < sizeof magic_cases / sizeof magic_cases[0]; ++i)
    {
        CHECK_EQ(led89_is_batch_header(magic_cases[i].bytes),
                 magic_cases[i].is_header);
        CHECK_EQ(led89_is_batch_footer(magic_cases[i].bytes),
                 magic_cases[i].is_footer);
        CHECK_EQ(led89_is_marker(magic_cases[i].bytes),
                 magic_cases[i].is_marker);
    }
}

static void test_bytes_equal(void)
{
    size_t i;

    for (i = 0u; i < sizeof bytes_cases / sizeof bytes_cases[0]; ++i)
    {
        CHECK_EQ(led89_bytes_equal(bytes_cases[i].a, bytes_cases[i].b,
                                   bytes_cases[i].n),
                 bytes_cases[i].expected);
    }
}

int main(void)
{
    test_scalars();
    test_current();
    test_manifest_sizes();
    test_manifest_roundtrip();
    test_manifest_rejections();
    test_part_header();
    test_part_header_rejections();
    test_batch_header();
    test_batch_footer();
    test_marker();
    test_sealed_footer();
    test_magic_predicates();
    test_bytes_equal();
    TEST_END;
}
