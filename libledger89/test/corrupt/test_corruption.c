/* test_corruption.c - structural corruption classification. */

#include <string.h>

#include "crash_util.h"
#include "test.h"

#define ACTIVE "ledger/part.0000000000000001"
#define MANIFEST "ledger/MANIFEST.0000000000000001"
#define CURRENT "ledger/CURRENT"

static void build(mfs *fs, led89_io *io)
{
    ledger89 *l;

    CHECK_EQ(cu_open(fs, io, &l), LEDGER89_OK);
    CHECK(cu_fill_range(l, 1ul, 3ul));
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    mfs_crash(fs);
    ledger89_close(l);
}

static void expect_open_fails(int op, size_t offset, unsigned char value)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    int rc;

    mfs_init(&fs);
    build(&fs, &io);
    CHECK(mfs_poke(&fs, op == 0 ? CURRENT : (op == 1 ? MANIFEST : ACTIVE),
                   offset, value) != 0);
    l = NULL;
    rc = cu_open(&fs, &io, &l);
    CHECK(rc == LEDGER89_ECORRUPT || rc == LEDGER89_EFORMAT);
    mfs_destroy(&fs);
}

static void sweep_current(void)
{
    size_t i;

    for (i = 0u; i < 20u; ++i)
    {
        expect_open_fails(0, i, 0xFFu);
    }
    /* Reserved bytes are ignored. */
    {
        mfs fs;
        led89_io io;
        ledger89 *l;

        mfs_init(&fs);
        build(&fs, &io);
        CHECK(mfs_poke(&fs, CURRENT, 24u, 0xFFu) != 0);
        CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
        ledger89_close(l);
        mfs_destroy(&fs);
    }
}

static void sweep_manifest(void)
{
    size_t i;

    for (i = 0u; i < 8u; ++i)
    {
        expect_open_fails(1, i, 0xFFu);
    }
    for (i = 8u; i < 60u; ++i)
    {
        expect_open_fails(1, i, 0xFFu);
    }
}

static void sweep_part_header(void)
{
    size_t i;

    for (i = 0u; i < 8u; ++i)
    {
        expect_open_fails(2, i, 0xFFu);
    }
    for (i = 24u; i < 52u; ++i)
    {
        expect_open_fails(2, i, 0xFFu);
    }
    for (i = 56u; i < 60u; ++i)
    {
        expect_open_fails(2, i, 0xFFu);
    }
}

static void sweep_baseline_marker(void)
{
    size_t i;

    /* The trailer bytes 120..127 are a scan aid; marker validity comes
     * from the magic and checksum, so corrupting only the trailer of the
     * baseline marker is tolerated. */
    for (i = 64u; i < 120u; ++i)
    {
        expect_open_fails(2, i, 0xFFu);
    }
}

static void corrupt_stable_footer(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    int rc;

    mfs_init(&fs);
    build(&fs, &io);
    /* Each record is its own batch: the first footer magic is at 169. */
    CHECK(mfs_poke(&fs, ACTIVE, 169u, (unsigned char)'X') != 0);
    l = NULL;
    rc = cu_open(&fs, &io, &l);
    CHECK_EQ(rc, LEDGER89_ECORRUPT);
    mfs_destroy(&fs);
}

static void corrupt_last_marker_falls_back(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_state st;

    mfs_init(&fs);
    build(&fs, &io);
    /* The only sync marker sits at 323; corrupt its checksum. */
    CHECK(mfs_poke(&fs, ACTIVE, 323u + 52u, 0xFFu) != 0);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.end, test_u64(1));
    CHECK_U64(st.stable_end, test_u64(1));
    ledger89_close(l);
    mfs_destroy(&fs);
}

static void garbage_tail_is_discarded(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    ledger89_state st;
    unsigned char junk[16];

    mfs_init(&fs);
    build(&fs, &io);
    memset(junk, 0x5A, sizeof junk);
    CHECK(mfs_insert(&fs, ACTIVE, 387u, junk, sizeof junk) != 0);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.end, test_u64(4));
    CHECK(cu_check_range(l, 1ul, 3ul));
    ledger89_close(l);
    mfs_destroy(&fs);
}

int main(void)
{
    sweep_current();
    sweep_manifest();
    sweep_part_header();
    sweep_baseline_marker();
    corrupt_stable_footer();
    corrupt_last_marker_falls_back();
    garbage_tail_is_discarded();
    TEST_END;
}
