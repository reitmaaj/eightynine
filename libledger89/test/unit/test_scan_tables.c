/* test_scan_tables.c - table-driven white-box coverage for the recovery
 * scan helpers, truncate helpers, and part record size helpers. Scan helpers
 * that need a handle are exercised through the deterministic model
 * filesystem with byte fixtures built by the library itself. */

#include <string.h>

#include "test.h"

#include "model_fs.h"

#include "ledger89_file.c"
#include "ledger89_recover.c"
#include "ledger89_segment.c"
#include "ledger89_truncate.c"

enum
{
    KIND_NONE = 0,
    KIND_CRC,
    KIND_UUID,
    KIND_FILE_ID,
    KIND_REVISION,
    KIND_MAGIC,
    KIND_VERSION
};

static const char scan_part[] = "ledger/part.0000000000000001";

struct error_case
{
    int in;
    int expected;
};

static const struct error_case scan_error_cases[] = {
    {LEDGER89_EIO, LEDGER89_EIO},
    {LEDGER89_ENOMEM, LEDGER89_ENOMEM},
    {LEDGER89_ECORRUPT, LEDGER89_ECORRUPT},
    {LEDGER89_EFORMAT, LEDGER89_ECORRUPT},
    {LEDGER89_ENOENT, LEDGER89_ECORRUPT},
    {LEDGER89_EINVAL, LEDGER89_ECORRUPT},
    {LEDGER89_ERANGE, LEDGER89_ECORRUPT},
    {LEDGER89_OK, LEDGER89_ECORRUPT}};

struct moff_case
{
    led89_u64 read_lo;
    size_t idx;
    led89_u64 expected;
};

static const struct moff_case moff_cases[] = {
    {(led89_u64)0, 56u, (led89_u64)0},
    {(led89_u64)56, 0u, (led89_u64)0},
    {(led89_u64)64, 56u, (led89_u64)64},
    {(led89_u64)64, 64u, (led89_u64)72},
    {(led89_u64)100, 3u, (led89_u64)47},
    {(led89_u64)4096, 64u, (led89_u64)4104}};

struct window_case
{
    led89_u64 hi;
    led89_u64 expected;
};

static const struct window_case window_lo_cases[] = {
    {(led89_u64)0, (led89_u64)64},
    {(led89_u64)63, (led89_u64)64},
    {(led89_u64)64, (led89_u64)64},
    {(led89_u64)4160, (led89_u64)64},
    {(led89_u64)4161, (led89_u64)65},
    {(led89_u64)8192, (led89_u64)4096},
    {~((led89_u64)0), ~((led89_u64)0) - (led89_u64)4096}};

static const struct window_case window_read_lo_cases[] = {
    {(led89_u64)0, (led89_u64)64},
    {(led89_u64)64, (led89_u64)64},
    {(led89_u64)71, (led89_u64)64},
    {(led89_u64)72, (led89_u64)65},
    {(led89_u64)4096, (led89_u64)4089},
    {~((led89_u64)0), ~((led89_u64)0) - (led89_u64)7}};

struct trailer_case
{
    unsigned char bytes[16];
    size_t limit;
    size_t expected;
};

static const struct trailer_case trailer_cases[] = {
    {{'8', '9', 'S', 'T', 'A', 'B', 'L', 'E', 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u},
     8u,
     0u},
    {{'8', '9', 'S', 'T', 'A', 'B', 'L', 'E', 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u},
     7u,
     (size_t)-1},
    {{'x', '8', '9', 'S', 'T', 'A', 'B', 'L', 'E', 0u, 0u, 0u, 0u, 0u, 0u, 0u},
     9u,
     1u},
    {{'8', '9', 'S', 'T', 'A', 'B', 'L', 'E', 'x', 0u, 0u, 0u, 0u, 0u, 0u, 0u},
     9u,
     0u},
    {{'8', '9', 'S', 'T', 'A', 'B', 'L', 'E', '8', '9', 'S', 'T', 'A', 'B', 'L',
      'E'},
     16u,
     8u},
    {{'8', '9', 'S', 'T', 'A', 'B', 'L', 'E', '8', '9', 'S', 'T', 'A', 'B', 'L',
      'E'},
     15u,
     0u},
    {{'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u},
     8u,
     (size_t)-1},
    {{0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u},
     16u,
     (size_t)-1}};

struct match_case
{
    int uuid_byte;
    int file_id;
    int revision;
    int expected;
};

static const struct match_case match_cases[] = {
    {-1, 7, 3, 1}, {0, 7, 3, 0}, {15, 7, 3, 0}, {-1, 8, 3, 0}, {-1, 7, 4, 0}};

struct off_case
{
    led89_u64 v;
    int expected;
};

static const struct off_case off_ok_cases[] = {{(led89_u64)0, 1},
                                               {(led89_u64)1, 1},
                                               {~((led89_u64)0) >> 1, 1},
                                               {((led89_u64)1 << 63), 0},
                                               {~((led89_u64)0), 0}};

struct rebase_case
{
    size_t part;
    size_t drop;
    size_t expected;
};

static const struct rebase_case rebase_cases[] = {
    {0u, 0u, 0u}, {1u, 1u, 0u}, {5u, 2u, 3u}, {9u, 3u, 6u}, {100u, 99u, 1u}};

struct desc_id_case
{
    led89_u64 file_id;
    led89_u64 first;
    led89_u64 end;
};

static const struct desc_id_case desc_id_cases[] = {
    {(led89_u64)1, (led89_u64)1, (led89_u64)10},
    {(led89_u64)42, (led89_u64)7, (led89_u64)9},
    {~((led89_u64)0), (led89_u64)0, (led89_u64)0}};

struct t_inc_case
{
    size_t in;
    size_t out;
};

static const struct t_inc_case t_inc_cases[] = {
    {0u, 1u}, {1u, 2u}, {4096u, 4097u}, {(size_t)-1, 0u}};

struct slice_data_case
{
    size_t size;
    int expected_null;
};

static const struct slice_data_case slice_data_cases[] = {
    {0u, 1}, {1u, 0}, {8u, 0}};

struct slice_size_case
{
    size_t size;
    led89_u64 expected;
};

static const struct slice_size_case slice_size_cases[] = {
    {0u, (led89_u64)0},
    {1u, (led89_u64)1},
    {5u, (led89_u64)5},
    {0xFFFFFFFFu, (led89_u64)0xFFFFFFFFu},
    {(size_t)-1, (led89_u64)(size_t)-1}};

struct record_bytes_case
{
    led89_u64 size;
    led89_u64 expected;
};

static const struct record_bytes_case record_bytes_cases[] = {
    {(led89_u64)0, (led89_u64)8},
    {(led89_u64)1, (led89_u64)9},
    {(led89_u64)5, (led89_u64)13},
    {(led89_u64)0xFFFFFFFFu, (led89_u64)0x100000007u},
    {~((led89_u64)0), (led89_u64)7}};

static const led89_u64 kept_parts[] = {(led89_u64)0, (led89_u64)0, (led89_u64)1,
                                       (led89_u64)2, (led89_u64)2};

struct kept_case
{
    size_t keep_sealed;
    size_t expected;
};

static const struct kept_case kept_cases[] = {
    {0u, 0u}, {1u, 2u}, {2u, 3u}, {3u, 5u}, {99u, 5u}};

struct last_marker_case
{
    unsigned long appends;
    int sync;
    unsigned long appends2;
    int sync2;
    int corrupt;
    int expected_rc;
    led89_u64 expected_end;
    led89_u64 expected_off;
};

static const struct last_marker_case last_marker_cases[] = {
    {0ul, 0, 0ul, 0, KIND_NONE, LEDGER89_OK, (led89_u64)1, (led89_u64)64},
    {1ul, 0, 0ul, 0, KIND_NONE, LEDGER89_OK, (led89_u64)1, (led89_u64)64},
    {1ul, 1, 0ul, 0, KIND_NONE, LEDGER89_OK, (led89_u64)2, (led89_u64)193},
    {2ul, 1, 0ul, 0, KIND_NONE, LEDGER89_OK, (led89_u64)3, (led89_u64)258},
    {2ul, 0, 0ul, 0, KIND_NONE, LEDGER89_OK, (led89_u64)1, (led89_u64)64},
    {1ul, 1, 1ul, 1, KIND_NONE, LEDGER89_OK, (led89_u64)3, (led89_u64)322},
    {2ul, 1, 0ul, 0, KIND_CRC, LEDGER89_OK, (led89_u64)1, (led89_u64)64},
    {2ul, 1, 0ul, 0, KIND_UUID, LEDGER89_OK, (led89_u64)1, (led89_u64)64},
    {2ul, 1, 0ul, 0, KIND_FILE_ID, LEDGER89_OK, (led89_u64)1, (led89_u64)64},
    {2ul, 1, 0ul, 0, KIND_REVISION, LEDGER89_OK, (led89_u64)1, (led89_u64)64},
    {2ul, 1, 0ul, 0, KIND_MAGIC, LEDGER89_OK, (led89_u64)1, (led89_u64)64},
    {2ul, 1, 0ul, 0, KIND_VERSION, LEDGER89_OK, (led89_u64)1, (led89_u64)64},
    {1ul, 1, 1ul, 1, KIND_CRC, LEDGER89_OK, (led89_u64)2, (led89_u64)193},
    {1ul, 1, 1ul, 1, KIND_UUID, LEDGER89_OK, (led89_u64)2, (led89_u64)193},
    {1ul, 1, 0ul, 0, KIND_CRC, LEDGER89_OK, (led89_u64)1, (led89_u64)64},
    {0ul, 0, 0ul, 0, KIND_CRC, LEDGER89_ECORRUPT, (led89_u64)0, (led89_u64)0},
    {0ul, 0, 0ul, 0, KIND_UUID, LEDGER89_ECORRUPT, (led89_u64)0, (led89_u64)0},
    {0ul, 0, 0ul, 0, KIND_MAGIC, LEDGER89_ECORRUPT, (led89_u64)0, (led89_u64)0},
    {0ul, 0, 0ul, 0, KIND_VERSION, LEDGER89_ECORRUPT, (led89_u64)0,
     (led89_u64)0}};

struct step_case
{
    unsigned long appends;
    int sync;
    size_t corrupt_off;
    led89_u64 pos;
    int expected_found;
    led89_u64 expected_next;
    led89_u64 expected_off;
    led89_u64 expected_end;
};

static const struct step_case step_cases[] = {
    {0ul, 0, 0u, (led89_u64)128, 1, (led89_u64)128, (led89_u64)64,
     (led89_u64)1},
    {0ul, 0, 0u, (led89_u64)64, 0, (led89_u64)0, (led89_u64)0, (led89_u64)0},
    {0ul, 0, 0u, (led89_u64)120, 0, (led89_u64)0, (led89_u64)0, (led89_u64)0},
    {0ul, 0, 0u, (led89_u64)72, 0, (led89_u64)0, (led89_u64)0, (led89_u64)0},
    {1ul, 1, 0u, (led89_u64)257, 1, (led89_u64)257, (led89_u64)193,
     (led89_u64)2},
    {63ul, 0, 0u, (led89_u64)4223, 1, (led89_u64)4223, (led89_u64)64,
     (led89_u64)1},
    {63ul, 0, 64u, (led89_u64)4223, 0, (led89_u64)127, (led89_u64)0,
     (led89_u64)0},
    {63ul, 0, 64u, (led89_u64)127, 0, (led89_u64)0, (led89_u64)0,
     (led89_u64)0}};

struct hit_case
{
    led89_u64 read_lo;
    size_t idx;
    int corrupt;
    int expected_hit;
    led89_u64 expected_off;
    led89_u64 expected_end;
};

static const struct hit_case hit_cases[] = {
    {(led89_u64)64, 56u, KIND_NONE, 1, (led89_u64)64, (led89_u64)1},
    {(led89_u64)64, 0u, KIND_NONE, 0, (led89_u64)0, (led89_u64)0},
    {(led89_u64)64, 64u, KIND_NONE, 0, (led89_u64)0, (led89_u64)0},
    {(led89_u64)64, 57u, KIND_NONE, 0, (led89_u64)0, (led89_u64)0},
    {(led89_u64)100, 20u, KIND_NONE, 1, (led89_u64)64, (led89_u64)1},
    {(led89_u64)120, 0u, KIND_NONE, 1, (led89_u64)64, (led89_u64)1},
    {(led89_u64)0, 64u, KIND_NONE, 0, (led89_u64)0, (led89_u64)0},
    {(led89_u64)64, 56u, KIND_CRC, 0, (led89_u64)0, (led89_u64)0},
    {(led89_u64)64, 56u, KIND_UUID, 0, (led89_u64)0, (led89_u64)0},
    {(led89_u64)64, 56u, KIND_FILE_ID, 0, (led89_u64)0, (led89_u64)0},
    {(led89_u64)64, 56u, KIND_REVISION, 0, (led89_u64)0, (led89_u64)0}};

static void scan_poke_flip(mfs *fs, size_t off)
{
    mfs_file *f;

    f = mfs_find(fs, scan_part);
    CHECK(f != NULL);
    if (f == NULL)
    {
        return;
    }
    CHECK(mfs_poke(fs, scan_part, off,
                   (unsigned char)(f->live_data[off] ^ 0x01u)) != 0);
}

static void scan_corrupt_marker(mfs *fs, size_t off, int kind)
{
    switch (kind)
    {
    case KIND_CRC:
        scan_poke_flip(fs, off + 52u);
        break;
    case KIND_UUID:
        scan_poke_flip(fs, off + 8u);
        break;
    case KIND_FILE_ID:
        scan_poke_flip(fs, off + 24u);
        break;
    case KIND_REVISION:
        scan_poke_flip(fs, off + 32u);
        break;
    case KIND_MAGIC:
        scan_poke_flip(fs, off);
        break;
    case KIND_VERSION:
        CHECK(mfs_poke(fs, scan_part, off + 48u, 3u) != 0);
        break;
    default:
        break;
    }
}

static void scan_fixture(mfs *fs, led89_io *io, ledger89 **l,
                         unsigned long appends, int sync,
                         unsigned long appends2, int sync2)
{
    ledger89_slice s;
    unsigned char byte;
    unsigned long i;

    mfs_init(fs);
    mfs_bind(io, fs);
    *l = NULL;
    CHECK_EQ(led89_open_io(l, "ledger",
                           LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE, io),
             LEDGER89_OK);
    if (*l == NULL)
    {
        return;
    }
    byte = (unsigned char)'a';
    s.data = &byte;
    s.size = 1u;
    for (i = 0ul; i < appends; ++i)
    {
        CHECK_EQ(ledger89_appendv(*l, &s, 1u, NULL), LEDGER89_OK);
    }
    if (sync != 0)
    {
        CHECK_EQ(ledger89_sync(*l, NULL), LEDGER89_OK);
    }
    for (i = 0ul; i < appends2; ++i)
    {
        CHECK_EQ(ledger89_appendv(*l, &s, 1u, NULL), LEDGER89_OK);
    }
    if (sync2 != 0)
    {
        CHECK_EQ(ledger89_sync(*l, NULL), LEDGER89_OK);
    }
}

static size_t scan_last_marker_off(unsigned long appends, int sync,
                                   unsigned long appends2, int sync2)
{
    size_t off;

    off = 64u;
    if (sync != 0)
    {
        off = 128u + ((size_t)appends * 65u);
    }
    if (sync2 != 0)
    {
        off = 128u + (((size_t)appends + (size_t)appends2) * 65u);
        if (sync != 0)
        {
            off += 64u;
        }
    }
    return off;
}

static void test_scan_error(void)
{
    size_t i;

    for (i = 0u; i < sizeof scan_error_cases / sizeof scan_error_cases[0]; ++i)
    {
        CHECK_EQ(led89_scan_error(scan_error_cases[i].in),
                 scan_error_cases[i].expected);
    }
}

static void test_trailer_moff(void)
{
    size_t i;

    for (i = 0u; i < sizeof moff_cases / sizeof moff_cases[0]; ++i)
    {
        CHECK(led89_trailer_moff(moff_cases[i].read_lo, moff_cases[i].idx) ==
              moff_cases[i].expected);
    }
}

static void test_windows(void)
{
    size_t i;

    for (i = 0u; i < sizeof window_lo_cases / sizeof window_lo_cases[0]; ++i)
    {
        CHECK(led89_window_lo(window_lo_cases[i].hi) ==
              window_lo_cases[i].expected);
    }
    for (i = 0u;
         i < sizeof window_read_lo_cases / sizeof window_read_lo_cases[0]; ++i)
    {
        CHECK(led89_window_read_lo(window_read_lo_cases[i].hi) ==
              window_read_lo_cases[i].expected);
    }
}

static void test_find_trailer(void)
{
    size_t i;

    for (i = 0u; i < sizeof trailer_cases / sizeof trailer_cases[0]; ++i)
    {
        CHECK_EQ(
            led89_find_trailer(trailer_cases[i].bytes, trailer_cases[i].limit),
            trailer_cases[i].expected);
    }
}

static void test_marker_matches(void)
{
    size_t i;

    for (i = 0u; i < sizeof match_cases / sizeof match_cases[0]; ++i)
    {
        ledger89 l;
        led89_part part;
        led89_marker m;

        memset(&l, 0, sizeof l);
        memset(&part, 0, sizeof part);
        memset(&m, 0, sizeof m);
        memset(l.id.bytes, 0x11, 16u);
        memset(m.uuid, 0x11, 16u);
        part.desc.file_id = (led89_u64)7;
        l.parts = &part;
        l.active_index = 0u;
        l.revision = (led89_u64)3;
        if (match_cases[i].uuid_byte >= 0)
        {
            m.uuid[match_cases[i].uuid_byte] ^= 0x01u;
        }
        m.file_id = (led89_u64)match_cases[i].file_id;
        m.revision = (led89_u64)match_cases[i].revision;
        CHECK_EQ(led89_marker_matches(&l, &m), match_cases[i].expected);
    }
}

static void test_off_ok(void)
{
    size_t i;

    for (i = 0u; i < sizeof off_ok_cases / sizeof off_ok_cases[0]; ++i)
    {
        CHECK_EQ(led89_off_ok(off_ok_cases[i].v), off_ok_cases[i].expected);
    }
}

static void test_dir_rebase(void)
{
    size_t i;

    for (i = 0u; i < sizeof rebase_cases / sizeof rebase_cases[0]; ++i)
    {
        led89_batch_dir_entry e;

        memset(&e, 0, sizeof e);
        e.part = (led89_u64)rebase_cases[i].part;
        led89_dir_rebase(&e, rebase_cases[i].drop);
        CHECK(e.part == (led89_u64)rebase_cases[i].expected);
    }
}

static void test_t_desc_id(void)
{
    size_t i;

    for (i = 0u; i < sizeof desc_id_cases / sizeof desc_id_cases[0]; ++i)
    {
        led89_part_desc d;

        d.file_id = desc_id_cases[i].file_id;
        d.first = desc_id_cases[i].first;
        d.end = desc_id_cases[i].end;
        CHECK(led89_t_desc_id(&d) == desc_id_cases[i].file_id);
    }
}

static void test_t_inc(void)
{
    size_t i;

    for (i = 0u; i < sizeof t_inc_cases / sizeof t_inc_cases[0]; ++i)
    {
        CHECK_EQ(led89_t_inc(t_inc_cases[i].in), t_inc_cases[i].out);
    }
}

static void test_t_slice_data(void)
{
    size_t i;

    for (i = 0u; i < sizeof slice_data_cases / sizeof slice_data_cases[0]; ++i)
    {
        ledger89 l;
        unsigned char buf[8];
        const void *p;

        memset(&l, 0, sizeof l);
        memset(buf, 0, sizeof buf);
        l.buf = buf;
        p = led89_t_slice_data(&l, slice_data_cases[i].size);
        if (slice_data_cases[i].expected_null != 0)
        {
            CHECK(p == NULL);
        }
        else
        {
            CHECK(p == (const void *)buf);
        }
    }
}

static void test_slice_size(void)
{
    size_t i;

    for (i = 0u; i < sizeof slice_size_cases / sizeof slice_size_cases[0]; ++i)
    {
        ledger89_slice s;

        s.data = NULL;
        s.size = slice_size_cases[i].size;
        CHECK(led89_slice_size(&s) == slice_size_cases[i].expected);
    }
}

static void test_record_bytes(void)
{
    size_t i;

    for (i = 0u; i < sizeof record_bytes_cases / sizeof record_bytes_cases[0];
         ++i)
    {
        CHECK(led89_record_bytes(record_bytes_cases[i].size) ==
              record_bytes_cases[i].expected);
    }
}

static void test_kept_entries(void)
{
    size_t i;
    ledger89 l;
    led89_batch_dir_entry dir[5];

    for (i = 0u; i < 5u; ++i)
    {
        memset(&dir[i], 0, sizeof dir[i]);
        dir[i].part = kept_parts[i];
    }
    memset(&l, 0, sizeof l);
    l.dir = dir;
    l.dir_count = 5u;
    for (i = 0u; i < sizeof kept_cases / sizeof kept_cases[0]; ++i)
    {
        CHECK_EQ(led89_kept_entries(&l, kept_cases[i].keep_sealed),
                 kept_cases[i].expected);
    }
    l.dir_count = 0u;
    CHECK_EQ(led89_kept_entries(&l, 0u), 0u);
}

static void test_find_last_marker(void)
{
    size_t i;

    for (i = 0u; i < sizeof last_marker_cases / sizeof last_marker_cases[0];
         ++i)
    {
        const struct last_marker_case *c;
        mfs fs;
        led89_io io;
        ledger89 *l;
        led89_marker m;
        led89_u64 off;
        led89_fd fd;
        int rc;

        c = &last_marker_cases[i];
        scan_fixture(&fs, &io, &l, c->appends, c->sync, c->appends2, c->sync2);
        if (l == NULL)
        {
            mfs_destroy(&fs);
            continue;
        }
        if (c->corrupt != KIND_NONE)
        {
            scan_corrupt_marker(&fs,
                                scan_last_marker_off(c->appends, c->sync,
                                                     c->appends2, c->sync2),
                                c->corrupt);
        }
        fd = l->parts[l->active_index].fd;
        memset(&m, 0, sizeof m);
        off = (led89_u64)0;
        rc = led89_find_last_marker(l, fd, l->parts[l->active_index].bytes, &m,
                                    &off);
        CHECK_EQ(rc, c->expected_rc);
        if (rc == LEDGER89_OK)
        {
            CHECK(m.end == c->expected_end);
            CHECK(off == c->expected_off);
        }
        ledger89_close(l);
        mfs_destroy(&fs);
    }
}

static void test_marker_step(void)
{
    size_t i;

    for (i = 0u; i < sizeof step_cases / sizeof step_cases[0]; ++i)
    {
        const struct step_case *c;
        mfs fs;
        led89_io io;
        ledger89 *l;
        led89_marker m;
        led89_u64 pos;
        led89_u64 off;
        led89_fd fd;
        int found;
        int rc;

        c = &step_cases[i];
        scan_fixture(&fs, &io, &l, c->appends, c->sync, 0ul, 0);
        if (l == NULL)
        {
            mfs_destroy(&fs);
            continue;
        }
        if (c->corrupt_off != 0u)
        {
            scan_poke_flip(&fs, c->corrupt_off + 52u);
        }
        fd = l->parts[l->active_index].fd;
        pos = c->pos;
        memset(&m, 0, sizeof m);
        off = (led89_u64)0;
        found = 0;
        rc = led89_marker_step(l, fd, l->parts[l->active_index].bytes, &pos, &m,
                               &off, &found);
        CHECK_EQ(rc, LEDGER89_OK);
        CHECK_EQ(found, c->expected_found);
        CHECK(pos == c->expected_next);
        if (found != 0)
        {
            CHECK(off == c->expected_off);
            CHECK(m.end == c->expected_end);
        }
        ledger89_close(l);
        mfs_destroy(&fs);
    }
}

static void test_scan_hit(void)
{
    size_t i;

    for (i = 0u; i < sizeof hit_cases / sizeof hit_cases[0]; ++i)
    {
        const struct hit_case *c;
        mfs fs;
        led89_io io;
        ledger89 *l;
        led89_marker m;
        led89_u64 off;
        led89_fd fd;
        int hit;

        c = &hit_cases[i];
        scan_fixture(&fs, &io, &l, 0ul, 0, 0ul, 0);
        if (l == NULL)
        {
            mfs_destroy(&fs);
            continue;
        }
        if (c->corrupt != KIND_NONE)
        {
            scan_corrupt_marker(&fs, 64u, c->corrupt);
        }
        fd = l->parts[l->active_index].fd;
        memset(&m, 0, sizeof m);
        off = (led89_u64)0;
        hit = led89_scan_hit(l, fd, c->read_lo, c->idx,
                             l->parts[l->active_index].bytes, &m, &off);
        CHECK_EQ(hit, c->expected_hit);
        if (hit != 0)
        {
            CHECK(off == c->expected_off);
            CHECK(m.end == c->expected_end);
        }
        ledger89_close(l);
        mfs_destroy(&fs);
    }
}

int main(void)
{
    test_scan_error();
    test_trailer_moff();
    test_windows();
    test_find_trailer();
    test_marker_matches();
    test_off_ok();
    test_dir_rebase();
    test_t_desc_id();
    test_t_inc();
    test_t_slice_data();
    test_slice_size();
    test_record_bytes();
    test_kept_entries();
    test_find_last_marker();
    test_marker_step();
    test_scan_hit();
    TEST_END;
}
