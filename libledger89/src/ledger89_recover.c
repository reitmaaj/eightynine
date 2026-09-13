/* ledger89_recover.c - directory scan, topology validation, and active
 * segment recovery.
 *
 * Recovery validates segment structure only: headers, sealed footers, and
 * name/range ordering. The active segment is scanned batch by batch; at the
 * first invalid position, a fully valid batch later in the file means
 * corruption, otherwise the remainder is a torn tail and is truncated. */

#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

#define LED89_SCAN_BUF 4096u
#define LED89_DIR_NAME_MAX 256u

/* --- pure helpers ----------------------------------------------------- */

static size_t led89_double(size_t value)
{
    return value * 2u;
}

static size_t led89_grow_cap(size_t cap, size_t need)
{
    while (cap < need)
    {
        cap = led89_double(cap);
    }
    return cap;
}

static int led89_is_empty(const ledger89 *l)
{
    return l->last_index < l->first_index;
}

/* Expected first index for the active segment: 0 means "keep the existing
 * header", which preserves a discarded base when no sealed segment remains. */
static led89_u64 led89_segment_next(const ledger89 *l)
{
    if (l->segment_count == 0u)
    {
        return 0u;
    }
    return l->segments[l->segment_count - 1u].last_index + 1u;
}

static led89_u64 led89_base_of(const ledger89 *l)
{
    if (l->segment_count == 0u)
    {
        return l->active_first;
    }
    return l->segments[0].first_index;
}

static led89_u64 led89_fix_last(const ledger89 *l)
{
    if (led89_is_empty(l) != 0)
    {
        return l->base - 1u;
    }
    return l->last_index;
}

/* --- segment table ---------------------------------------------------- */

static int led89_segments_reserve(ledger89 *l, size_t need)
{
    led89_segment *p;
    size_t cap;

    if (need <= l->segment_cap)
    {
        return LEDGER89_OK;
    }
    cap = l->segment_cap;
    if (cap == 0u)
    {
        cap = 8u;
    }
    cap = led89_grow_cap(cap, need);
    p = (led89_segment *)realloc(l->segments, cap * sizeof *p);
    if (p == NULL)
    {
        return LEDGER89_ERR_NOMEM;
    }
    l->segments = p;
    l->segment_cap = cap;
    return LEDGER89_OK;
}

int led89_segments_add(ledger89 *l, const char *name, led89_u64 first,
                       led89_u64 last)
{
    led89_segment *seg;
    int rc;

    rc = led89_segments_reserve(l, l->segment_count + 1u);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    seg = &l->segments[l->segment_count];
    seg->first_index = first;
    seg->last_index = last;
    memcpy(seg->name, name, strlen(name) + 1u);
    l->segment_count += 1u;
    return LEDGER89_OK;
}

static size_t led89_segments_pos(const ledger89 *l, size_t upto,
                                 led89_u64 first)
{
    size_t j;

    j = upto;
    while (j > 0u)
    {
        if (l->segments[j - 1u].first_index <= first)
        {
            break;
        }
        --j;
    }
    return j;
}

static void led89_segments_insert(ledger89 *l, size_t upto)
{
    led89_segment tmp;
    size_t j;

    tmp = l->segments[upto];
    j = led89_segments_pos(l, upto, tmp.first_index);
    memmove(&l->segments[j + 1u], &l->segments[j], (upto - j) * sizeof tmp);
    l->segments[j] = tmp;
}

static void led89_segments_sort(ledger89 *l)
{
    size_t i;

    for (i = 1u; i < l->segment_count; ++i)
    {
        led89_segments_insert(l, i);
    }
}

/* --- directory scan --------------------------------------------------- */

static int led89_take_entry(ledger89 *l, const char *name, int *has_active)
{
    led89_u64 first;
    int rc;

    if (strcmp(name, LED89_ACTIVE_NAME) == 0)
    {
        *has_active = 1;
        return LEDGER89_OK;
    }
    if (strcmp(name, LED89_LOCK_NAME) == 0)
    {
        return LEDGER89_OK;
    }
    rc = led89_name_parse(name, &first);
    if (rc == 0)
    {
        return LEDGER89_OK;
    }
    rc = led89_segments_add(l, name, first, 0u);
    return rc;
}

static int led89_scan_dir_close(ledger89 *l, led89_dir *dir, int rc)
{
    int close_rc;

    close_rc = l->io->list_close(l->io->ctx, dir);
    (void)close_rc;
    return rc;
}

static int led89_scan_dir(ledger89 *l, int *has_active)
{
    led89_dir *dir;
    char name[LED89_DIR_NAME_MAX];
    int done;
    int rc;

    *has_active = 0;
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
            rc = led89_scan_dir_close(l, dir, rc);
            return rc;
        }
        if (done == 0)
        {
            rc = led89_take_entry(l, name, has_active);
            if (rc != LEDGER89_OK)
            {
                rc = led89_scan_dir_close(l, dir, rc);
                return rc;
            }
        }
    }
    rc = l->io->list_close(l->io->ctx, dir);
    return rc;
}

/* --- sealed segment validation ---------------------------------------- */

static int led89_check_sealed_body(ledger89 *l, led89_fd fd, led89_segment *seg)
{
    unsigned char hdr[LED89_SEGMENT_HEADER_SIZE];
    unsigned char foot[LED89_SEGMENT_FOOTER_SIZE];
    led89_seg_header sh;
    led89_seg_footer sf;
    led89_u64 size;
    int rc;

    rc = l->io->size(l->io->ctx, fd, &size);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (size <
        (led89_u64)(LED89_SEGMENT_HEADER_SIZE + LED89_SEGMENT_FOOTER_SIZE))
    {
        return LEDGER89_ERR_CORRUPT;
    }
    rc = l->io->pread(l->io->ctx, fd, hdr, sizeof hdr, 0u);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_seg_header_decode(hdr, &sh);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (sh.flags != 0u)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    seg->first_index = sh.first_index;
    rc = l->io->pread(l->io->ctx, fd, foot, sizeof foot,
                      size - (led89_u64)LED89_SEGMENT_FOOTER_SIZE);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_seg_footer_decode(foot, &sf);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (sf.body_size != size - (led89_u64)(LED89_SEGMENT_HEADER_SIZE +
                                           LED89_SEGMENT_FOOTER_SIZE))
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (sf.last_index < sh.first_index)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (sf.record_count != sf.last_index - sh.first_index + 1u)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    seg->last_index = sf.last_index;
    return LEDGER89_OK;
}

static int led89_check_sealed(ledger89 *l, led89_segment *seg)
{
    led89_fd fd;
    int rc;
    int close_rc;

    rc = led89_path_join(l->scratch, l->scratch_cap, l->path, seg->name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->open(l->io->ctx, l->scratch, LED89_OPEN_READ, &fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_check_sealed_body(l, fd, seg);
    close_rc = l->io->close(l->io->ctx, fd);
    (void)close_rc;
    return rc;
}

static int led89_check_gap(const ledger89 *l, size_t i)
{
    if (l->segments[i].first_index != l->segments[i - 1u].last_index + 1u)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    return LEDGER89_OK;
}

/* --- active segment --------------------------------------------------- */

int led89_active_path(ledger89 *l)
{
    int rc;

    rc =
        led89_path_join(l->scratch, l->scratch_cap, l->path, LED89_ACTIVE_NAME);
    return rc;
}

int led89_write_active_header(ledger89 *l, led89_u64 first)
{
    unsigned char hdr[LED89_SEGMENT_HEADER_SIZE];
    led89_seg_header sh;
    led89_fd fd;
    int rc;

    sh.first_index = first;
    sh.flags = 0u;
    led89_seg_header_encode(hdr, &sh);
    led89_active_close(l);
    rc = led89_path_join(l->scratch, l->scratch_cap, l->path, LED89_TMP_NAME);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->open(l->io->ctx, l->scratch,
                     LED89_OPEN_READ | LED89_OPEN_WRITE | LED89_OPEN_CREATE,
                     &fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->truncate(l->io->ctx, fd, 0u);
    if (rc != LEDGER89_OK)
    {
        l->io->close(l->io->ctx, fd);
        return rc;
    }
    rc = l->io->pwrite(l->io->ctx, fd, hdr, sizeof hdr, 0u);
    if (rc != LEDGER89_OK)
    {
        l->io->close(l->io->ctx, fd);
        return rc;
    }
    rc = l->io->sync(l->io->ctx, fd);
    if (rc != LEDGER89_OK)
    {
        l->io->close(l->io->ctx, fd);
        return rc;
    }
    rc = l->io->close(l->io->ctx, fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_install_tmp(l, LED89_ACTIVE_NAME);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_active_path(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->open(l->io->ctx, l->scratch, LED89_OPEN_READ | LED89_OPEN_WRITE,
                     &l->active_fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->active_open = 1;
    return LEDGER89_OK;
}

static led89_u64 led89_or_one(led89_u64 v)
{
    if (v == 0u)
    {
        return 1u;
    }
    return v;
}

static int led89_active_repair(ledger89 *l, led89_u64 size, led89_u64 expected)
{
    int rc;

    if (size == (led89_u64)LED89_SEGMENT_HEADER_SIZE)
    {
        rc = led89_write_active_header(l, led89_or_one(expected));
        return rc;
    }
    return LEDGER89_ERR_CORRUPT;
}

static int led89_active_open(ledger89 *l, led89_u64 expected)
{
    unsigned char hdr[LED89_SEGMENT_HEADER_SIZE];
    led89_seg_header sh;
    led89_u64 size;
    int rc;

    rc = led89_active_path(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->open(l->io->ctx, l->scratch,
                     LED89_OPEN_READ | LED89_OPEN_WRITE | LED89_OPEN_CREATE,
                     &l->active_fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->active_open = 1;
    rc = l->io->size(l->io->ctx, l->active_fd, &size);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (size < (led89_u64)LED89_SEGMENT_HEADER_SIZE)
    {
        rc = led89_write_active_header(l, led89_or_one(expected));
        return rc;
    }
    rc = l->io->pread(l->io->ctx, l->active_fd, hdr, sizeof hdr, 0u);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_seg_header_decode(hdr, &sh);
    if (rc != LEDGER89_OK)
    {
        rc = led89_active_repair(l, size, expected);
        return rc;
    }
    if (sh.flags != 0u)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (expected == 0u)
    {
        return LEDGER89_OK;
    }
    if (sh.first_index == expected)
    {
        return LEDGER89_OK;
    }
    rc = led89_active_repair(l, size, expected);
    return rc;
}

int led89_active_create(ledger89 *l, led89_u64 first)
{
    int rc;

    rc = led89_write_active_header(l, first);
    return rc;
}

static int led89_active_read_header(ledger89 *l)
{
    unsigned char hdr[LED89_SEGMENT_HEADER_SIZE];
    led89_seg_header sh;
    int rc;

    rc = l->io->pread(l->io->ctx, l->active_fd, hdr, sizeof hdr, 0u);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_seg_header_decode(hdr, &sh);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    l->active_first = sh.first_index;
    return LEDGER89_OK;
}

/* --- active batch scan ------------------------------------------------ */

typedef struct led89_scan_state
{
    led89_u64 at;
    led89_u64 size;
    led89_u64 last_valid;
    led89_u64 last_index;
    led89_u64 records;
} led89_scan_state;

static int led89_scan_chunk(const led89_io *io, led89_fd fd, unsigned char *buf,
                            size_t cap, led89_u64 *at, led89_u64 *left,
                            led89_u32 *rec_crc, led89_u32 *batch_crc)
{
    size_t chunk;
    int rc;

    chunk = led89_chunk_size(cap, *left);
    rc = io->pread(io->ctx, fd, buf, chunk, *at);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *rec_crc = led89_crc32c(*rec_crc, buf, chunk);
    *batch_crc = led89_crc32c(*batch_crc, buf, chunk);
    *at += (led89_u64)chunk;
    *left -= (led89_u64)chunk;
    return LEDGER89_OK;
}

static int led89_scan_record(const led89_io *io, led89_fd fd, led89_u64 *offset,
                             led89_u64 end, led89_u32 *batch_crc,
                             led89_rec_header *rh)
{
    unsigned char hdr[LED89_RECORD_HEADER_SIZE];
    unsigned char buf[LED89_SCAN_BUF];
    unsigned char crcbuf[LED89_RECORD_CRC_SIZE];
    led89_u32 rec_crc;
    led89_u32 stored;
    led89_u64 left;
    led89_u64 at;
    int rc;

    if (end - *offset < (led89_u64)LED89_RECORD_HEADER_SIZE)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    rc = io->pread(io->ctx, fd, hdr, sizeof hdr, *offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_rec_header_decode(hdr, rh);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    *batch_crc = led89_crc32c(*batch_crc, hdr, sizeof hdr);
    rec_crc = led89_crc32c(0u, hdr, sizeof hdr);
    at = *offset + (led89_u64)LED89_RECORD_HEADER_SIZE;
    left = (led89_u64)rh->payload_size;
    if (left > end - at)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (end - at - left < (led89_u64)LED89_RECORD_CRC_SIZE)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    while (left > 0u)
    {
        rc = led89_scan_chunk(io, fd, buf, sizeof buf, &at, &left, &rec_crc,
                              batch_crc);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    if (end - at < (led89_u64)LED89_RECORD_CRC_SIZE)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    rc = io->pread(io->ctx, fd, crcbuf, sizeof crcbuf, at);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *batch_crc = led89_crc32c(*batch_crc, crcbuf, sizeof crcbuf);
    stored = led89_get_u32(crcbuf);
    if (stored != rec_crc)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    *offset = at + (led89_u64)LED89_RECORD_CRC_SIZE;
    return LEDGER89_OK;
}

static int led89_scan_batch_record(const led89_io *io, led89_fd fd,
                                   led89_u64 *at, led89_u64 batch_end,
                                   led89_u64 *expect, led89_u64 *last_seen,
                                   led89_u32 *bcrc)
{
    led89_rec_header rh;
    int rc;

    rc = led89_scan_record(io, fd, at, batch_end, bcrc, &rh);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (rh.index != *expect)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    *expect += 1u;
    *last_seen = rh.index;
    return LEDGER89_OK;
}

static int led89_scan_footer(const led89_io *io, led89_fd fd, led89_u64 at,
                             const led89_batch_header *bh, led89_u64 last_seen,
                             led89_u32 bcrc)
{
    unsigned char foot[LED89_BATCH_FOOTER_SIZE];
    led89_batch_footer bf;
    led89_u32 crc;
    int rc;

    rc = io->pread(io->ctx, fd, foot, sizeof foot, at);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_batch_footer_decode(foot, &bf);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (bf.record_count != bh->record_count)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (bf.last_index != last_seen)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    crc = led89_crc32c(bcrc, foot, 16u);
    if (crc != bf.crc)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    return LEDGER89_OK;
}

int led89_scan_batch(const led89_io *io, led89_fd fd, led89_u64 offset,
                     led89_u64 end, led89_u64 *next, led89_u64 *first,
                     led89_u64 *last, led89_u32 *count)
{
    unsigned char hdr[LED89_BATCH_HEADER_SIZE];
    led89_batch_header bh;
    led89_u64 at;
    led89_u64 expect;
    led89_u64 last_seen;
    led89_u64 batch_end;
    led89_u32 bcrc;
    led89_u32 i;
    int rc;

    if (end - offset < (led89_u64)LED89_BATCH_HEADER_SIZE)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    rc = io->pread(io->ctx, fd, hdr, sizeof hdr, offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_batch_header_decode(hdr, &bh);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (bh.batch_bytes > end - offset)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    batch_end = offset + bh.batch_bytes;
    bcrc = led89_crc32c(0u, hdr, sizeof hdr);
    at = offset + (led89_u64)LED89_BATCH_HEADER_SIZE;
    expect = bh.first_index;
    last_seen = bh.first_index;
    for (i = 0u; i < bh.record_count; ++i)
    {
        rc = led89_scan_batch_record(io, fd, &at, batch_end, &expect,
                                     &last_seen, &bcrc);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    if (at != batch_end - (led89_u64)LED89_BATCH_FOOTER_SIZE)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    rc = led89_scan_footer(io, fd, at, &bh, last_seen, bcrc);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *next = batch_end;
    *first = bh.first_index;
    *last = last_seen;
    *count = bh.record_count;
    return LEDGER89_OK;
}

static int led89_try_batch(const led89_io *io, led89_fd fd, led89_u64 at,
                           led89_u64 end, int *found)
{
    unsigned char magic[4];
    led89_u64 next;
    led89_u64 first;
    led89_u64 last;
    led89_u32 count;
    int rc;

    rc = io->pread(io->ctx, fd, magic, sizeof magic, at);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (led89_is_batch_header(magic) == 0)
    {
        return LEDGER89_OK;
    }
    rc = led89_scan_batch(io, fd, at, end, &next, &first, &last, &count);
    if (rc == LEDGER89_OK)
    {
        *found = 1;
        return LEDGER89_OK;
    }
    if (rc == LEDGER89_ERR_CORRUPT)
    {
        return LEDGER89_OK;
    }
    return rc;
}

static int led89_find_valid_batch(const led89_io *io, led89_fd fd,
                                  led89_u64 start, led89_u64 end, int *found)
{
    led89_u64 at;
    int rc;

    *found = 0;
    at = start;
    while (at + 4u <= end)
    {
        rc = led89_try_batch(io, fd, at, end, found);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
        if (*found != 0)
        {
            return LEDGER89_OK;
        }
        ++at;
    }
    return LEDGER89_OK;
}

static int led89_scan_torn(ledger89 *l, led89_scan_state *s, int *done)
{
    int found;
    int rc;

    rc = led89_find_valid_batch(l->io, l->active_fd, s->at + 1u, s->size,
                                &found);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (found != 0)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    rc = l->io->truncate(l->io->ctx, l->active_fd, s->last_valid);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->sync(l->io->ctx, l->active_fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *done = 1;
    return LEDGER89_OK;
}

static int led89_scan_accept(led89_scan_state *s, led89_u64 first,
                             led89_u64 last, led89_u32 count, led89_u64 next)
{
    if (first != s->last_index + 1u)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    s->last_index = last;
    s->records += (led89_u64)count;
    s->at = next;
    s->last_valid = next;
    return LEDGER89_OK;
}

static int led89_scan_next_batch(ledger89 *l, led89_scan_state *s, int *done)
{
    led89_u64 next;
    led89_u64 first;
    led89_u64 last;
    led89_u32 count;
    int rc;

    *done = 0;
    rc = led89_scan_batch(l->io, l->active_fd, s->at, s->size, &next, &first,
                          &last, &count);
    if (rc == LEDGER89_OK)
    {
        rc = led89_scan_accept(s, first, last, count, next);
        return rc;
    }
    if (rc == LEDGER89_ERR_CORRUPT)
    {
        rc = led89_scan_torn(l, s, done);
        return rc;
    }
    return rc;
}

static int led89_scan_active(ledger89 *l)
{
    led89_scan_state s;
    int done;
    int rc;

    rc = l->io->size(l->io->ctx, l->active_fd, &s.size);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    s.at = (led89_u64)LED89_SEGMENT_HEADER_SIZE;
    s.last_valid = s.at;
    s.last_index = l->active_first - 1u;
    s.records = 0u;
    done = 0;
    while (done == 0)
    {
        if (s.at >= s.size)
        {
            done = 1;
        }
        else
        {
            rc = led89_scan_next_batch(l, &s, &done);
            if (rc != LEDGER89_OK)
            {
                return rc;
            }
        }
    }
    l->active_offset = s.last_valid;
    l->active_records = s.records;
    l->last_index = s.last_index;
    return LEDGER89_OK;
}

int led89_recover(ledger89 *l)
{
    int has_active;
    size_t i;
    int rc;

    rc = led89_scan_dir(l, &has_active);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    (void)has_active;
    for (i = 0u; i < l->segment_count; ++i)
    {
        rc = led89_check_sealed(l, &l->segments[i]);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    led89_segments_sort(l);
    for (i = 1u; i < l->segment_count; ++i)
    {
        rc = led89_check_gap(l, i);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    rc = led89_active_open(l, led89_segment_next(l));
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_active_read_header(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->base = led89_base_of(l);
    rc = led89_scan_active(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->first_index = l->base;
    l->last_index = led89_fix_last(l);
    /* Bytes recovered from a previous incarnation may still be only in the
     * page cache; force the next sync to fsync the active segment. */
    l->dirty = 1;
    return LEDGER89_OK;
}
