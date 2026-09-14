/* ledger89_recover.c - open-time recovery.
 *
 * CURRENT selects the committed physical topology; the latest valid stable
 * marker selects the committed data frontier inside the active part.
 * Recovery never synthesizes a mixture of old and new structural state. */
#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

#define LED89_SCAN_CHUNK 4096u
#define LED89_TRAILER_OVERLAP 7u

static const unsigned char led89_trailer[LED89_MARKER_TRAILER_SIZE] = {
    '8', '9', 'S', 'T', 'A', 'B', 'L', 'E'};

static int led89_create_name_kind(const char *name)
{
    led89_u64 ignored;
    int rc;

    if (strcmp(name, LED89_LOCK_NAME) == 0)
    {
        return 0;
    }
    rc = led89_part_name_parse(name, &ignored);
    if (rc != 0)
    {
        return 1;
    }
    rc = led89_manifest_name_parse(name, &ignored);
    if (rc != 0)
    {
        return 1;
    }
    if (strcmp(name, LED89_CURRENT_TMP_NAME) == 0)
    {
        return 1;
    }
    return 2;
}

static void led89_unlink_name(ledger89 *l, const char *name)
{
    int rc;

    rc = led89_path_join(l->scratch, l->scratch_cap, l->path, name);
    if (rc == LEDGER89_OK)
    {
        led89_unlink_quiet(l, l->scratch);
    }
}

static led89_u64 led89_r_min(void)
{
    return (led89_u64)LED89_PART_HEADER_SIZE;
}

static int led89_scan_error(int rc)
{
    if (rc == LEDGER89_EIO)
    {
        return rc;
    }
    if (rc == LEDGER89_ENOMEM)
    {
        return rc;
    }
    return LEDGER89_ECORRUPT;
}

static int led89_marker_matches(const ledger89 *l, const led89_marker *m)
{
    if (led89_bytes_equal(m->uuid, l->id.bytes, 16u) == 0)
    {
        return 0;
    }
    if (m->file_id != l->parts[l->active_index].desc.file_id)
    {
        return 0;
    }
    if (m->revision != l->revision)
    {
        return 0;
    }
    return 1;
}

static size_t led89_find_trailer(const unsigned char *buf, size_t limit)
{
    size_t i;

    if (limit < (size_t)LED89_MARKER_TRAILER_SIZE)
    {
        return (size_t)-1;
    }
    i = limit - (size_t)LED89_MARKER_TRAILER_SIZE;
    for (;;)
    {
        if (memcmp(buf + i, led89_trailer, (size_t)LED89_MARKER_TRAILER_SIZE) ==
            0)
        {
            return i;
        }
        if (i == 0u)
        {
            break;
        }
        --i;
    }
    return (size_t)-1;
}

static led89_u64 led89_trailer_moff(led89_u64 read_lo, size_t idx)
{
    return led89_u64_left(led89_at_add(read_lo, (led89_u64)idx), (led89_u64)56);
}

static int led89_try_marker(ledger89 *l, led89_fd fd, led89_u64 moff,
                            led89_u64 size, led89_marker *marker,
                            led89_u64 *offset)
{
    unsigned char mb[LED89_MARKER_SIZE];
    led89_marker m;
    int rc;

    if (moff < led89_r_min())
    {
        return 0;
    }
    if (led89_at_add(moff, (led89_u64)LED89_MARKER_SIZE) > size)
    {
        return 0;
    }
    rc = l->io->pread(l->io->ctx, fd, mb, sizeof mb, moff);
    if (rc != LEDGER89_OK)
    {
        return 0;
    }
    rc = led89_marker_decode(mb, &m);
    if (rc != LEDGER89_OK)
    {
        return 0;
    }
    if (led89_marker_matches(l, &m) == 0)
    {
        return 0;
    }
    *marker = m;
    *offset = moff;
    return 1;
}

static int led89_scan_hit(ledger89 *l, led89_fd fd, led89_u64 read_lo,
                          size_t idx, led89_u64 size, led89_marker *marker,
                          led89_u64 *offset)
{
    led89_u64 moff;
    int rc;

    moff = led89_trailer_moff(read_lo, idx);
    rc = led89_try_marker(l, fd, moff, size, marker, offset);
    return rc;
}

static int led89_scan_window(ledger89 *l, led89_fd fd, led89_u64 read_lo,
                             led89_u64 hi, led89_u64 size, led89_marker *marker,
                             led89_u64 *offset, int *found)
{
    size_t len;
    size_t idx;
    int rc;

    *found = 0;
    rc = led89_u64_to_size(led89_u64_left(hi, read_lo), &len);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ECORRUPT;
    }
    rc = led89_buf_reserve(l, len);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->pread(l->io->ctx, fd, l->buf, len, read_lo);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    idx = led89_find_trailer(l->buf, len);
    while (idx != (size_t)-1)
    {
        if (led89_at_add(read_lo, (led89_u64)idx) >= (led89_u64)56)
        {
            int hit;

            hit = led89_scan_hit(l, fd, read_lo, idx, size, marker, offset);
            if (hit != 0)
            {
                *found = 1;
                return LEDGER89_OK;
            }
        }
        idx = led89_find_trailer(l->buf, idx);
    }
    return LEDGER89_OK;
}

static led89_u64 led89_window_lo(led89_u64 hi)
{
    led89_u64 min;

    min = led89_r_min();
    if (hi > led89_at_add(min, (led89_u64)LED89_SCAN_CHUNK))
    {
        return led89_u64_left(hi, (led89_u64)LED89_SCAN_CHUNK);
    }
    return min;
}

static led89_u64 led89_window_read_lo(led89_u64 lo)
{
    led89_u64 min;

    min = led89_r_min();
    if (lo > led89_at_add(min, (led89_u64)LED89_TRAILER_OVERLAP))
    {
        return led89_u64_left(lo, (led89_u64)LED89_TRAILER_OVERLAP);
    }
    return min;
}

static int led89_marker_step(ledger89 *l, led89_fd fd, led89_u64 size,
                             led89_u64 *pos, led89_marker *marker,
                             led89_u64 *offset, int *found)
{
    led89_u64 lo;
    led89_u64 read_lo;
    int rc;

    lo = led89_window_lo(*pos);
    read_lo = led89_window_read_lo(lo);
    rc = led89_scan_window(l, fd, read_lo, *pos, size, marker, offset, found);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (*found != 0)
    {
        return LEDGER89_OK;
    }
    if (lo == led89_r_min())
    {
        *pos = (led89_u64)0;
        return LEDGER89_OK;
    }
    *pos = lo;
    return LEDGER89_OK;
}

int led89_find_last_marker(ledger89 *l, led89_fd fd, led89_u64 size,
                           led89_marker *marker, led89_u64 *offset)
{
    led89_u64 pos;

    pos = size;
    while (pos > led89_r_min())
    {
        int found;
        int rc;

        rc = led89_marker_step(l, fd, size, &pos, marker, offset, &found);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
        if (found != 0)
        {
            return LEDGER89_OK;
        }
    }
    return LEDGER89_ECORRUPT;
}

static int led89_scan_marker_at(ledger89 *l, led89_fd fd, size_t part_index,
                                int sealed, led89_u64 off, led89_u64 *expected)
{
    unsigned char mb[LED89_MARKER_SIZE];
    led89_marker m;
    int rc;

    rc = l->io->pread(l->io->ctx, fd, mb, sizeof mb, off);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    rc = led89_marker_decode(mb, &m);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ECORRUPT;
    }
    if (led89_bytes_equal(m.uuid, l->id.bytes, 16u) == 0)
    {
        return LEDGER89_ECORRUPT;
    }
    if (m.file_id != l->parts[part_index].desc.file_id)
    {
        return LEDGER89_ECORRUPT;
    }
    if (sealed == 0)
    {
        if (m.revision != l->revision)
        {
            return LEDGER89_ECORRUPT;
        }
    }
    if (m.end != *expected)
    {
        return LEDGER89_ECORRUPT;
    }
    return LEDGER89_OK;
}

static int led89_scan_batch_at(ledger89 *l, led89_fd fd, size_t part_index,
                               led89_u64 *off, led89_u64 limit,
                               led89_u64 *expected)
{
    unsigned char hb[LED89_BATCH_HEADER_SIZE];
    unsigned char fb[LED89_BATCH_FOOTER_SIZE];
    led89_batch_header bh;
    led89_batch_footer bf;
    int rc;

    if (led89_at_add(*off, (led89_u64)LED89_MIN_BATCH_BYTES) > limit)
    {
        return LEDGER89_ECORRUPT;
    }
    rc = l->io->pread(l->io->ctx, fd, hb, sizeof hb, *off);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    rc = led89_batch_header_decode(hb, &bh);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ECORRUPT;
    }
    if (bh.first != *expected)
    {
        return LEDGER89_ECORRUPT;
    }
    if (bh.bytes > led89_u64_left(limit, *off))
    {
        return LEDGER89_ECORRUPT;
    }
    rc = l->io->pread(
        l->io->ctx, fd, fb, sizeof fb,
        led89_u64_left(led89_at_add(*off, bh.bytes), (led89_u64)sizeof fb));
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    rc = led89_batch_footer_decode(fb, &bf);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ECORRUPT;
    }
    if (bf.count != bh.count)
    {
        return LEDGER89_ECORRUPT;
    }
    if (bf.last != led89_u64_left(led89_at_add(*expected, (led89_u64)bh.count),
                                  (led89_u64)1))
    {
        return LEDGER89_ECORRUPT;
    }
    rc = led89_dir_add(l, part_index, *expected, (led89_u64)bh.count, *off,
                       bh.bytes);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *expected = led89_at_add(*expected, (led89_u64)bh.count);
    *off = led89_at_add(*off, bh.bytes);
    return LEDGER89_OK;
}

static int led89_scan_batches(ledger89 *l, led89_fd fd, size_t part_index,
                              int sealed, led89_u64 limit, int have_chosen,
                              led89_u64 chosen_off, led89_u64 *expected_out)
{
    led89_u64 off;
    led89_u64 expected;
    int rc;

    off = (led89_u64)128;
    expected = l->parts[part_index].desc.first;
    while (off < limit)
    {
        unsigned char mag[8];

        if (led89_at_add(off, (led89_u64)8) > limit)
        {
            return LEDGER89_ECORRUPT;
        }
        rc = l->io->pread(l->io->ctx, fd, mag, sizeof mag, off);
        if (rc != LEDGER89_OK)
        {
            return LEDGER89_EIO;
        }
        if (led89_is_marker(mag) != 0)
        {
            if (led89_at_add(off, (led89_u64)LED89_MARKER_SIZE) > limit)
            {
                return LEDGER89_ECORRUPT;
            }
            rc =
                led89_scan_marker_at(l, fd, part_index, sealed, off, &expected);
            if (rc != LEDGER89_OK)
            {
                return rc;
            }
            off = led89_at_add(off, (led89_u64)LED89_MARKER_SIZE);
            if (have_chosen != 0)
            {
                if (off ==
                    led89_at_add(chosen_off, (led89_u64)LED89_MARKER_SIZE))
                {
                    break;
                }
            }
        }
        else if (led89_is_batch_header(mag) != 0)
        {
            rc = led89_scan_batch_at(l, fd, part_index, &off, limit, &expected);
            if (rc != LEDGER89_OK)
            {
                return rc;
            }
        }
        else
        {
            return LEDGER89_ECORRUPT;
        }
    }
    if (have_chosen != 0)
    {
        if (off != led89_at_add(chosen_off, (led89_u64)LED89_MARKER_SIZE))
        {
            return LEDGER89_ECORRUPT;
        }
    }
    else
    {
        if (off != limit)
        {
            return LEDGER89_ECORRUPT;
        }
    }
    *expected_out = expected;
    return LEDGER89_OK;
}

static int led89_rw_flags(void)
{
    return LED89_OPEN_READ | LED89_OPEN_WRITE;
}

static void led89_set_bytes(led89_part *p, led89_u64 v)
{
    p->bytes = v;
}

static int led89_scan_header(ledger89 *l, led89_fd fd, size_t part_index)
{
    unsigned char hdr[LED89_PART_HEADER_SIZE];
    led89_part_header ph;
    int rc;

    rc = l->io->pread(l->io->ctx, fd, hdr, sizeof hdr, (led89_u64)0);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    rc = led89_part_header_decode(hdr, &ph);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ECORRUPT;
    }
    if (led89_bytes_equal(ph.uuid, l->id.bytes, 16u) == 0)
    {
        return LEDGER89_ECORRUPT;
    }
    if (ph.file_id != l->parts[part_index].desc.file_id)
    {
        return LEDGER89_ECORRUPT;
    }
    if (ph.first != l->parts[part_index].desc.first)
    {
        return LEDGER89_ECORRUPT;
    }
    return LEDGER89_OK;
}

static int led89_scan_baseline(ledger89 *l, led89_fd fd, size_t part_index,
                               int sealed)
{
    unsigned char bm[LED89_MARKER_SIZE];
    led89_marker baseline;
    int rc;

    rc = l->io->pread(l->io->ctx, fd, bm, sizeof bm,
                      (led89_u64)LED89_PART_HEADER_SIZE);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    rc = led89_marker_decode(bm, &baseline);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ECORRUPT;
    }
    if (led89_bytes_equal(baseline.uuid, l->id.bytes, 16u) == 0)
    {
        return LEDGER89_ECORRUPT;
    }
    if (baseline.file_id != l->parts[part_index].desc.file_id)
    {
        return LEDGER89_ECORRUPT;
    }
    if (baseline.end != l->parts[part_index].desc.first)
    {
        return LEDGER89_ECORRUPT;
    }
    if (sealed == 0)
    {
        if (baseline.revision != l->revision)
        {
            return LEDGER89_ECORRUPT;
        }
    }
    return LEDGER89_OK;
}

static int led89_scan_footer(ledger89 *l, led89_fd fd, size_t part_index,
                             led89_u64 size, led89_sealed_footer *sf)
{
    unsigned char fbuf[LED89_SEALED_FOOTER_SIZE];
    int rc;

    if (size < (led89_u64)LED89_PART_HEADER_SIZE +
                   (led89_u64)LED89_MARKER_SIZE +
                   (led89_u64)LED89_SEALED_FOOTER_SIZE)
    {
        return LEDGER89_ECORRUPT;
    }
    rc =
        l->io->pread(l->io->ctx, fd, fbuf, sizeof fbuf,
                     led89_u64_left(size, (led89_u64)LED89_SEALED_FOOTER_SIZE));
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    rc = led89_sealed_footer_decode(fbuf, sf);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ECORRUPT;
    }
    if (led89_bytes_equal(sf->uuid, l->id.bytes, 16u) == 0)
    {
        return LEDGER89_ECORRUPT;
    }
    if (sf->file_id != l->parts[part_index].desc.file_id)
    {
        return LEDGER89_ECORRUPT;
    }
    if (sf->first != l->parts[part_index].desc.first)
    {
        return LEDGER89_ECORRUPT;
    }
    if (sf->end != l->parts[part_index].desc.end)
    {
        return LEDGER89_ECORRUPT;
    }
    if (sf->records != led89_u64_left(sf->end, sf->first))
    {
        return LEDGER89_ECORRUPT;
    }
    return LEDGER89_OK;
}

static int led89_trim_active(ledger89 *l, led89_fd fd, led89_u64 size,
                             led89_u64 stable)
{
    int rc;

    if (stable >= size)
    {
        return LEDGER89_OK;
    }
    rc = l->io->truncate(l->io->ctx, fd, stable);
    if (rc == LEDGER89_OK)
    {
        rc = l->io->sync(l->io->ctx, fd);
    }
    if (rc != LEDGER89_OK)
    {
        rc = led89_io_error(l, LEDGER89_EIO);
        return rc;
    }
    return LEDGER89_OK;
}

static int led89_scan_sealed(ledger89 *l, led89_fd fd, size_t part_index,
                             led89_u64 size, led89_sealed_footer *sf,
                             led89_u64 *limit)
{
    int rc;

    rc = led89_scan_footer(l, fd, part_index, size, sf);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *limit = led89_u64_left(size, (led89_u64)LED89_SEALED_FOOTER_SIZE);
    return LEDGER89_OK;
}

static int led89_scan_active(ledger89 *l, led89_fd fd, size_t part_index,
                             led89_u64 size, led89_u64 *stable_bytes,
                             led89_u64 *expected_out)
{
    led89_marker chosen;
    led89_u64 chosen_off;
    led89_u64 limit;
    int rc;

    rc = led89_find_last_marker(l, fd, size, &chosen, &chosen_off);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ECORRUPT;
    }
    limit = led89_at_add(chosen_off, (led89_u64)LED89_MARKER_SIZE);
    *stable_bytes = limit;
    rc = led89_scan_batches(l, fd, part_index, 0, limit, 1, chosen_off,
                            expected_out);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (*expected_out != chosen.end)
    {
        return LEDGER89_ECORRUPT;
    }
    return LEDGER89_OK;
}

int led89_part_scan(ledger89 *l, size_t part_index, int sealed,
                    led89_u64 *stable_bytes)
{
    led89_part *p;
    led89_fd fd;
    led89_u64 size;
    led89_u64 limit;
    led89_u64 expected;
    led89_sealed_footer sf;
    int flags;
    int rc;

    memset(&sf, 0, sizeof sf);
    p = &l->parts[part_index];
    flags = LED89_OPEN_READ;
    if (l->writable != 0)
    {
        flags = led89_rw_flags();
    }
    rc = led89_part_open(l, p->desc.file_id, flags, &fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->size(l->io->ctx, fd, &size);
    if (rc == LEDGER89_OK)
    {
        if (size < (led89_u64)128)
        {
            rc = LEDGER89_ECORRUPT;
        }
    }
    if (rc == LEDGER89_OK)
    {
        rc = led89_scan_header(l, fd, part_index);
    }
    if (rc == LEDGER89_OK)
    {
        rc = led89_scan_baseline(l, fd, part_index, sealed);
    }
    expected = (led89_u64)0;
    limit = (led89_u64)0;
    if (rc == LEDGER89_OK)
    {
        if (sealed != 0)
        {
            rc = led89_scan_sealed(l, fd, part_index, size, &sf, &limit);
        }
        else
        {
            rc = led89_scan_active(l, fd, part_index, size, stable_bytes,
                                   &expected);
        }
    }
    if (rc == LEDGER89_OK)
    {
        if (sealed != 0)
        {
            rc = led89_scan_batches(l, fd, part_index, 1, limit, 0,
                                    (led89_u64)0, &expected);
            if (rc == LEDGER89_OK)
            {
                if (expected != sf.end)
                {
                    rc = LEDGER89_ECORRUPT;
                }
            }
        }
    }
    if (rc != LEDGER89_OK)
    {
        led89_close_quiet(l, fd);
        return rc;
    }
    p->fd = fd;
    p->open = 1;
    p->bytes = size;
    p->sealed = sealed;
    if (sealed != 0)
    {
        led89_part_close(l, p);
        return LEDGER89_OK;
    }
    p->desc.end = expected;
    if (l->writable != 0)
    {
        rc = led89_trim_active(l, fd, size, *stable_bytes);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
        led89_set_bytes(p, *stable_bytes);
    }
    return LEDGER89_OK;
}

int led89_clean_dir_for_create(ledger89 *l)
{
    led89_dir *dir;
    char name[LED89_NAME_MAX];
    int done;
    int rc;

    rc = l->io->list_open(l->io->ctx, l->path, &dir);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    done = 0;
    while (done == 0)
    {
        rc = l->io->list_next(l->io->ctx, dir, name, sizeof name, &done);
        if (rc != LEDGER89_OK)
        {
            led89_list_close_quiet(l, dir);
            return rc;
        }
        if (done != 0)
        {
            break;
        }
        rc = led89_create_name_kind(name);
        if (rc == 0)
        {
            continue;
        }
        if (rc == 2)
        {
            led89_list_close_quiet(l, dir);
            return LEDGER89_ECORRUPT;
        }
        led89_unlink_name(l, name);
    }
    led89_list_close_quiet(l, dir);
    return LEDGER89_OK;
}

int led89_create_ledger(ledger89 *l)
{
    led89_manifest m;
    size_t idx;
    int rc;

    rc = l->io->entropy(l->io->ctx, l->id.bytes, 16u);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    l->revision = (led89_u64)0;
    l->first = (led89_u64)1;
    l->stable_end = (led89_u64)1;
    l->end = (led89_u64)1;
    l->generation = (led89_u64)1;
    l->next_file_id = (led89_u64)2;
    rc = led89_part_create(l, (led89_u64)1, (led89_u64)1, (led89_u64)0, &idx);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->active_index = idx;
    m.generation = (led89_u64)1;
    memcpy(m.uuid, l->id.bytes, 16u);
    m.revision = (led89_u64)0;
    m.first = (led89_u64)1;
    m.sealed_count = 0u;
    m.sealed = NULL;
    m.active = l->parts[idx].desc;
    rc = led89_manifest_publish(l, &m);
    if (rc == LEDGER89_OK)
    {
        rc = led89_current_publish(l, (led89_u64)1);
    }
    if (rc != LEDGER89_OK)
    {
        rc = led89_io_error(l, rc);
        return rc;
    }
    return LEDGER89_OK;
}

static led89_u64 led89_max_id(led89_u64 a, led89_u64 b)
{
    if (a > b)
    {
        return a;
    }
    return b;
}

static led89_u64 led89_max_ref_id(const led89_manifest *m)
{
    led89_u64 max_id;
    led89_u32 i;

    max_id = m->active.file_id;
    for (i = 0u; i < m->sealed_count; ++i)
    {
        max_id = led89_max_id(max_id, m->sealed[i].file_id);
    }
    return max_id;
}

static void led89_set_next_id(ledger89 *l, const led89_manifest *m)
{
    led89_u64 max_id;

    max_id = led89_max_ref_id(m);
    l->next_file_id = led89_u64_inc(max_id);
}

static void led89_set_part(led89_part *p, const led89_part_desc *d, int sealed)
{
    p->desc = *d;
    p->sealed = sealed;
    p->open = 0;
    p->fd = -1;
    p->bytes = (led89_u64)0;
}

int led89_recover(ledger89 *l)
{
    led89_current cur;
    led89_manifest m;
    led89_u64 stable;
    size_t i;
    int rc;

    rc = led89_current_read(l, &cur);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_manifest_read(l, cur.generation, &m);
    if (rc == LEDGER89_ENOENT)
    {
        return LEDGER89_ECORRUPT;
    }
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (m.generation != cur.generation)
    {
        led89_manifest_free(&m);
        return LEDGER89_ECORRUPT;
    }
    memcpy(l->id.bytes, m.uuid, 16u);
    l->revision = m.revision;
    l->first = m.first;
    l->generation = m.generation;
    rc = led89_handle_reserve_parts(l, (size_t)m.sealed_count + 1u);
    if (rc != LEDGER89_OK)
    {
        led89_manifest_free(&m);
        return rc;
    }
    for (i = 0u; i < (size_t)m.sealed_count; ++i)
    {
        led89_set_part(&l->parts[i], &m.sealed[i], 1);
    }
    led89_set_part(&l->parts[m.sealed_count], &m.active, 0);
    l->active_index = (size_t)m.sealed_count;
    l->part_count = (size_t)m.sealed_count + 1u;
    for (i = 0u; i < (size_t)m.sealed_count; ++i)
    {
        rc = led89_part_scan(l, i, 1, &stable);
        if (rc != LEDGER89_OK)
        {
            led89_manifest_free(&m);
            return led89_scan_error(rc);
        }
    }
    rc = led89_part_scan(l, l->active_index, 0, &stable);
    if (rc != LEDGER89_OK)
    {
        led89_manifest_free(&m);
        return led89_scan_error(rc);
    }
    l->stable_end = l->parts[l->active_index].desc.end;
    l->end = l->stable_end;
    l->dirty = 0;
    if (l->writable != 0)
    {
        rc = led89_gc_orphans(l, &m);
        if (rc != LEDGER89_OK)
        {
            led89_manifest_free(&m);
            return rc;
        }
    }
    else
    {
        led89_set_next_id(l, &m);
    }
    led89_manifest_free(&m);
    return LEDGER89_OK;
}
