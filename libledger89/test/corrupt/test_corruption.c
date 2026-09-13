/* test_corruption.c - X-series: structural and semantic corruption.
 *
 * Fixed pre-state layout (records appended one at a time, records=0):
 *
 *   sealed ...0001.seg: header 32, four 85-byte batches, footer 48 (size 420)
 *   active.seg:         header 32, two 85-byte batches (size 202)
 *   each batch:         24-byte header, 37-byte record, 24-byte footer
 *   record offsets:     56, 141, 226, 311 (sealed); 56, 141 (active) */

#include <string.h>

#include "test.h"

#include "crash_util.h"
#include "model_fs.h"

#define SEALED "ledger/00000000000000000001.seg"
#define ACTIVE "ledger/active.seg"

#define SEG_HDR 32u
#define BATCH 85u
#define BATCH_HDR 24u
#define REC_BYTES 37u
#define SEG_FOOT 48u

static int iterate_all(ledger89 *l)
{
    ledger89_iter *it;
    ledger89_view v;
    int rc;

    it = NULL;
    rc = ledger89_iter_open(l, 0ul, 0ul, &it);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    for (;;)
    {
        rc = ledger89_iter_next(it, &v);
        if (rc != LEDGER89_OK)
        {
            break;
        }
    }
    ledger89_iter_close(it);
    return rc;
}

static int build_base(mfs *base)
{
    led89_io io;
    ledger89 *l;

    mfs_init(base);
    CHECK_EQ(cu_open(base, &io, &l, 0ul), LEDGER89_OK);
    if (l == NULL)
    {
        return 0;
    }
    CHECK(cu_fill(l, 4ul) != 0);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(l), LEDGER89_OK);
    CHECK(cu_fill_range(l, 5ul, 6ul) != 0);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);
    return 1;
}

static int flip(mfs *fs, const char *name, size_t off)
{
    mfs_file *f;

    f = mfs_find(fs, name);
    if (f == NULL)
    {
        return 0;
    }
    if (off >= f->live_size)
    {
        return 0;
    }
    f->live_data[off] = (unsigned char)(f->live_data[off] ^ 0xFFu);
    return 1;
}

static ledger89_index sealed_record_at(size_t off)
{
    size_t i;

    for (i = 0u; i < 4u; ++i)
    {
        size_t start;

        start = SEG_HDR + (BATCH * i) + BATCH_HDR;
        if (off >= start)
        {
            if (off < start + REC_BYTES)
            {
                return (ledger89_index)(i + 1u);
            }
        }
    }
    return 0ul;
}

static void expect_open_corrupt(mfs *base, const char *name, size_t off)
{
    mfs fs;
    led89_io io;
    ledger89 *l;

    mfs_init(&fs);
    mfs_clone(&fs, base);
    CHECK(flip(&fs, name, off) != 0);
    l = NULL;
    CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_ERR_CORRUPT);
    CHECK(l == NULL);
    mfs_destroy(&fs);
}

static void expect_read_corrupt(mfs *base, const char *name, size_t off,
                                ledger89_index index)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_view v;

    mfs_init(&fs);
    mfs_clone(&fs, base);
    CHECK(flip(&fs, name, off) != 0);
    l = NULL;
    CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_OK);
    if (l == NULL)
    {
        mfs_destroy(&fs);
        return;
    }
    CHECK_EQ(ledger89_read(l, index, &v), LEDGER89_ERR_CORRUPT);
    CHECK_EQ(iterate_all(l), LEDGER89_ERR_CORRUPT);
    ledger89_close(l);
    mfs_destroy(&fs);
}

static void expect_framing_corrupt(mfs *base, const char *name, size_t off,
                                   ledger89_index index)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_view v;
    int rc;

    mfs_init(&fs);
    mfs_clone(&fs, base);
    CHECK(flip(&fs, name, off) != 0);
    l = NULL;
    CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_OK);
    if (l == NULL)
    {
        mfs_destroy(&fs);
        return;
    }
    rc = ledger89_read(l, index, &v);
    CHECK(rc == LEDGER89_OK || rc == LEDGER89_ERR_CORRUPT);
    CHECK_EQ(iterate_all(l), LEDGER89_ERR_CORRUPT);
    ledger89_close(l);
    mfs_destroy(&fs);
}

static void expect_iter_corrupt(mfs *base, const char *name, size_t off,
                                ledger89_index index)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_view v;

    mfs_init(&fs);
    mfs_clone(&fs, base);
    CHECK(flip(&fs, name, off) != 0);
    l = NULL;
    CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_OK);
    if (l == NULL)
    {
        mfs_destroy(&fs);
        return;
    }
    CHECK_EQ(ledger89_read(l, index, &v), LEDGER89_OK);
    CHECK_EQ(iterate_all(l), LEDGER89_ERR_CORRUPT);
    ledger89_close(l);
    mfs_destroy(&fs);
}

static void expect_tail_discard(mfs *base, size_t off)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_view v;

    mfs_init(&fs);
    mfs_clone(&fs, base);
    CHECK(flip(&fs, ACTIVE, off) != 0);
    l = NULL;
    CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_OK);
    if (l == NULL)
    {
        mfs_destroy(&fs);
        return;
    }
    CHECK_EQ(ledger89_last_index(l), 5ul);
    CHECK_EQ(ledger89_read(l, 6ul, &v), LEDGER89_ERR_NOTFOUND);
    CHECK_EQ(cu_check_range(l, 1ul, 5ul), 1);
    ledger89_close(l);
    mfs_destroy(&fs);
}

static void craft_size(mfs *fs, const char *name, size_t rec_off,
                       led89_u32 size)
{
    mfs_file *f;
    unsigned char hdr[LED89_RECORD_HEADER_SIZE];
    led89_rec_header rh;

    f = mfs_find(fs, name);
    memcpy(hdr, f->live_data + rec_off, sizeof hdr);
    (void)led89_rec_header_decode(hdr, &rh);
    rh.payload_size = size;
    led89_rec_header_encode(hdr, &rh);
    memcpy(f->live_data + rec_off, hdr, sizeof hdr);
}

static void craft_index(mfs *fs, const char *name, size_t rec_off,
                        led89_u64 index)
{
    mfs_file *f;
    unsigned char hdr[LED89_RECORD_HEADER_SIZE];
    unsigned char payload[64];
    unsigned char crc[LED89_RECORD_CRC_SIZE];
    led89_rec_header rh;
    led89_u32 c;

    f = mfs_find(fs, name);
    memcpy(hdr, f->live_data + rec_off, sizeof hdr);
    (void)led89_rec_header_decode(hdr, &rh);
    memcpy(payload, f->live_data + rec_off + LED89_RECORD_HEADER_SIZE,
           rh.payload_size);
    rh.index = index;
    led89_rec_header_encode(hdr, &rh);
    c = led89_crc32c(0u, hdr, sizeof hdr);
    c = led89_crc32c(c, payload, rh.payload_size);
    led89_put_u32(crc, c);
    memcpy(f->live_data + rec_off, hdr, sizeof hdr);
    memcpy(f->live_data + rec_off + LED89_RECORD_HEADER_SIZE + rh.payload_size,
           crc, sizeof crc);
}

static void craft_count_zero(mfs *fs, const char *name, size_t batch_off)
{
    mfs_file *f;

    f = mfs_find(fs, name);
    led89_put_u32(f->live_data + batch_off + 4u, 0u);
}

static void craft_flags(mfs *fs, const char *name, led89_u32 flags)
{
    mfs_file *f;
    unsigned char hdr[LED89_SEGMENT_HEADER_SIZE];
    led89_seg_header sh;

    f = mfs_find(fs, name);
    memcpy(hdr, f->live_data, sizeof hdr);
    (void)led89_seg_header_decode(hdr, &sh);
    sh.flags = flags;
    led89_seg_header_encode(hdr, &sh);
    memcpy(f->live_data, hdr, sizeof hdr);
}

static void craft_footer_range(mfs *fs, const char *name)
{
    mfs_file *f;
    unsigned char foot[LED89_SEGMENT_FOOTER_SIZE];
    led89_seg_footer sf;
    size_t size;

    f = mfs_find(fs, name);
    size = f->live_size;
    (void)led89_seg_footer_decode(
        f->live_data + size - LED89_SEGMENT_FOOTER_SIZE, &sf);
    sf.last_index = 0u;
    sf.record_count = 1u;
    led89_seg_footer_encode(foot, &sf);
    memcpy(f->live_data + size - LED89_SEGMENT_FOOTER_SIZE, foot, sizeof foot);
}

static void fill_random(mfs *fs, const char *name, unsigned long seed)
{
    mfs_file *f;
    size_t i;

    f = mfs_find(fs, name);
    for (i = 0u; i < f->live_size; ++i)
    {
        seed = (seed * 1103515245ul) + 12345ul;
        f->live_data[i] = (unsigned char)((seed >> 16) & 0xFFu);
    }
}

int main(void)
{
    mfs base;
    size_t off;
    size_t i;

    CHECK(build_base(&base) != 0);

    /* Sealed header and segment footer: rejected at open. */
    for (off = 0u; off < SEG_HDR; ++off)
    {
        expect_open_corrupt(&base, SEALED, off);
    }
    for (off = SEG_HDR + (4u * BATCH); off < SEG_HDR + (4u * BATCH) + SEG_FOOT;
         ++off)
    {
        expect_open_corrupt(&base, SEALED, off);
    }

    /* Sealed batches: header and record flips fail the read; footer flips
     * fail iteration only. */
    for (i = 0u; i < 4u; ++i)
    {
        size_t start;

        start = SEG_HDR + (BATCH * i);
        for (off = start; off < start + BATCH_HDR; ++off)
        {
            expect_framing_corrupt(&base, SEALED, off,
                                   (ledger89_index)(i + 1u));
        }
        for (off = start + BATCH_HDR; off < start + BATCH_HDR + REC_BYTES;
             ++off)
        {
            expect_read_corrupt(&base, SEALED, off, (ledger89_index)(i + 1u));
        }
        for (off = start + BATCH_HDR + REC_BYTES; off < start + BATCH; ++off)
        {
            expect_iter_corrupt(&base, SEALED, off, (ledger89_index)(i + 1u));
        }
    }

    /* Active header and history: rejected at open; the last batch is the
     * recoverable tail and is discarded. */
    for (off = 0u; off < 32u; ++off)
    {
        expect_open_corrupt(&base, ACTIVE, off);
    }
    for (off = 32u; off < 117u; ++off)
    {
        expect_open_corrupt(&base, ACTIVE, off);
    }
    for (off = 117u; off < 202u; ++off)
    {
        expect_tail_discard(&base, off);
    }

    /* X01/X02: impossible payload lengths. */
    {
        mfs fs;
        led89_io io;
        ledger89 *l;
        ledger89_view v;

        mfs_init(&fs);
        mfs_clone(&fs, &base);
        craft_size(&fs, SEALED, 141u, 0xFFFFFFFFu);
        l = NULL;
        CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_OK);
        CHECK_EQ(ledger89_read(l, 2ul, &v), LEDGER89_ERR_CORRUPT);
        ledger89_close(l);
        mfs_destroy(&fs);

        mfs_init(&fs);
        mfs_clone(&fs, &base);
        craft_size(&fs, SEALED, 141u, 1000000u);
        l = NULL;
        CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_OK);
        CHECK_EQ(ledger89_read(l, 2ul, &v), LEDGER89_ERR_CORRUPT);
        ledger89_close(l);
        mfs_destroy(&fs);
    }

    /* X03/X04/X05: valid checksum, impossible index. */
    {
        static const led89_u64 bad[] = {1u, 4u, 3u};
        size_t k;

        for (k = 0u; k < 3u; ++k)
        {
            mfs fs;
            led89_io io;
            ledger89 *l;
            ledger89_view v;

            mfs_init(&fs);
            mfs_clone(&fs, &base);
            craft_index(&fs, SEALED, 141u, bad[k]);
            l = NULL;
            CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_OK);
            CHECK_EQ(ledger89_read(l, 2ul, &v), LEDGER89_ERR_CORRUPT);
            ledger89_close(l);
            mfs_destroy(&fs);
        }
    }

    /* X06: malformed batch count. */
    {
        mfs fs;
        led89_io io;
        ledger89 *l;
        ledger89_view v;

        mfs_init(&fs);
        mfs_clone(&fs, &base);
        craft_count_zero(&fs, SEALED, SEG_HDR);
        l = NULL;
        CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_OK);
        CHECK_EQ(ledger89_read(l, 1ul, &v), LEDGER89_ERR_CORRUPT);
        ledger89_close(l);
        mfs_destroy(&fs);
    }

    /* X07: valid checksums, impossible structure. */
    {
        mfs fs;
        led89_io io;
        ledger89 *l;

        mfs_init(&fs);
        mfs_clone(&fs, &base);
        craft_flags(&fs, ACTIVE, 1u);
        l = NULL;
        CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_ERR_CORRUPT);
        mfs_destroy(&fs);

        mfs_init(&fs);
        mfs_clone(&fs, &base);
        craft_footer_range(&fs, SEALED);
        l = NULL;
        CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_ERR_CORRUPT);
        mfs_destroy(&fs);
    }

    /* X08: garbage after the active tail is truncated. */
    {
        mfs fs;
        led89_io io;
        ledger89 *l;

        mfs_init(&fs);
        mfs_clone(&fs, &base);
        CHECK(mfs_insert(&fs, ACTIVE, mfs_live_size(&fs, ACTIVE),
                         "\xFF\x00\xAA\x55", 4u) != 0);
        l = NULL;
        CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_OK);
        CHECK_EQ(ledger89_last_index(l), 6ul);
        CHECK_EQ(cu_check_range(l, 1ul, 6ul), 1);
        CHECK_EQ(mfs_live_size(&fs, ACTIVE), 202u);
        ledger89_close(l);
        mfs_destroy(&fs);
    }

    /* X09: garbage inside the active prefix is corruption. */
    {
        mfs fs;
        led89_io io;
        ledger89 *l;

        mfs_init(&fs);
        mfs_clone(&fs, &base);
        CHECK(mfs_insert(&fs, ACTIVE, 117u, "\xFF\x00\xAA\x55", 4u) != 0);
        l = NULL;
        CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_ERR_CORRUPT);
        mfs_destroy(&fs);
    }

    /* X10: wholly random files. */
    {
        mfs fs;
        led89_io io;
        ledger89 *l;

        mfs_init(&fs);
        mfs_clone(&fs, &base);
        fill_random(&fs, SEALED, 1ul);
        l = NULL;
        CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_ERR_CORRUPT);
        mfs_destroy(&fs);

        mfs_init(&fs);
        mfs_clone(&fs, &base);
        fill_random(&fs, ACTIVE, 2ul);
        l = NULL;
        CHECK_EQ(cu_open(&fs, &io, &l, 0ul), LEDGER89_ERR_CORRUPT);
        mfs_destroy(&fs);
    }

    /* Random single-byte fuzz: never a crash, never an invented record. */
    for (i = 0u; i < 200u; ++i)
    {
        mfs fs;
        led89_io io;
        ledger89 *l;
        unsigned long seed;
        size_t off2;
        const char *name;
        int rc;

        mfs_init(&fs);
        mfs_clone(&fs, &base);
        seed = (unsigned long)i + 7ul;
        seed = (seed * 1103515245ul) + 12345ul;
        if ((seed & 1ul) != 0ul)
        {
            name = SEALED;
        }
        else
        {
            name = ACTIVE;
        }
        seed = (seed * 1103515245ul) + 12345ul;
        off2 = (size_t)((seed >> 16) % mfs_live_size(&fs, name));
        CHECK(flip(&fs, name, off2) != 0);
        l = NULL;
        rc = cu_open(&fs, &io, &l, 0ul);
        CHECK(rc == LEDGER89_OK || rc == LEDGER89_ERR_CORRUPT);
        if (rc == LEDGER89_OK)
        {
            int irc;

            irc = iterate_all(l);
            CHECK(irc == LEDGER89_END || irc == LEDGER89_ERR_CORRUPT);
            ledger89_close(l);
        }
        mfs_destroy(&fs);
    }

    mfs_destroy(&base);

    TEST_END;
}
