/* ledger89_read.c - random read over the in-memory batch directory. */
#include <string.h>

#include "ledger89_internal.h"

static size_t led89_mid(size_t lo, size_t hi)
{
    return lo + ((hi - lo) / 2u);
}

static size_t led89_inc_size(size_t v)
{
    return v + 1u;
}

static led89_batch_dir_entry *led89_dir_probe(ledger89 *l, size_t lo, size_t hi,
                                              size_t *mid_out)
{
    size_t mid;

    mid = led89_mid(lo, hi);
    *mid_out = mid;
    return &l->dir[mid];
}

int led89_dir_find(ledger89 *l, led89_u64 index, size_t *entry)
{
    size_t lo;
    size_t hi;

    lo = 0u;
    hi = l->dir_count;
    while (lo < hi)
    {
        size_t mid;
        led89_batch_dir_entry *e;

        e = led89_dir_probe(l, lo, hi, &mid);
        if (index < e->first)
        {
            hi = mid;
        }
        else if (index >= e->first + e->count)
        {
            lo = led89_inc_size(mid);
        }
        else
        {
            *entry = mid;
            return LEDGER89_OK;
        }
    }
    return LEDGER89_ENOENT;
}

static void led89_part_take(led89_part *p, led89_fd *fd, int *opened)
{
    *fd = p->fd;
    *opened = 0;
}

static int led89_part_fd(ledger89 *l, size_t part_index, led89_fd *fd,
                         int *opened)
{
    led89_part *p;
    int rc;

    p = &l->parts[part_index];
    if (p->open != 0)
    {
        led89_part_take(p, fd, opened);
        return LEDGER89_OK;
    }
    rc = led89_part_open(l, p->desc.file_id, LED89_OPEN_READ, fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *opened = 1;
    return LEDGER89_OK;
}

static void led89_skip_one(led89_u64 *offset, led89_u64 *skip, led89_u32 len)
{
    *offset = *offset + (led89_u64)8 + (led89_u64)len;
    *skip = *skip - (led89_u64)1;
}

static led89_u64 led89_next_off(led89_u64 offset, size_t size)
{
    return offset + (led89_u64)8 + (led89_u64)size;
}

static led89_u64 led89_first_off(const led89_batch_dir_entry *e)
{
    return e->offset + (led89_u64)LED89_BATCH_HEADER_SIZE;
}

static int led89_body_offset(ledger89 *l, led89_fd fd,
                             const led89_batch_dir_entry *e, led89_u64 index,
                             led89_u64 *offset_out)
{
    unsigned char lenbuf[LED89_RECORD_LENGTH_SIZE];
    led89_u64 end;
    led89_u64 offset;
    led89_u64 skip;
    led89_u32 len;
    int rc;

    end = e->offset + e->bytes;
    offset = e->offset + (led89_u64)LED89_BATCH_HEADER_SIZE;
    skip = index - e->first;
    while (skip > (led89_u64)0)
    {
        if (offset + (led89_u64)8 > end)
        {
            return LEDGER89_ECORRUPT;
        }
        rc = l->io->pread(l->io->ctx, fd, lenbuf, sizeof lenbuf, offset);
        if (rc != LEDGER89_OK)
        {
            return LEDGER89_EIO;
        }
        len = led89_get_u32(lenbuf);
        if (offset + (led89_u64)8 + (led89_u64)len > end)
        {
            return LEDGER89_ECORRUPT;
        }
        led89_skip_one(&offset, &skip, len);
    }
    *offset_out = offset;
    return LEDGER89_OK;
}

static int led89_body_read(ledger89 *l, led89_fd fd,
                           const led89_batch_dir_entry *e, led89_u64 offset,
                           void *data_out, size_t capacity, size_t *size_out)
{
    unsigned char lenbuf[LED89_RECORD_LENGTH_SIZE];
    unsigned char crcbuf[LED89_RECORD_CRC_SIZE];
    led89_u64 end;
    led89_u32 len;
    led89_u32 stored;
    led89_u32 crc;
    int rc;

    end = e->offset + e->bytes;
    rc = l->io->pread(l->io->ctx, fd, lenbuf, sizeof lenbuf, offset);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    len = led89_get_u32(lenbuf);
    if (offset + (led89_u64)8 + (led89_u64)len > end)
    {
        return LEDGER89_ECORRUPT;
    }
    *size_out = (size_t)len;
    if (data_out == NULL)
    {
        return LEDGER89_OK;
    }
    if (capacity < (size_t)len)
    {
        return LEDGER89_ETOOSMALL;
    }
    if (len > 0u)
    {
        rc = l->io->pread(l->io->ctx, fd, data_out, (size_t)len,
                          offset + (led89_u64)LED89_RECORD_LENGTH_SIZE);
        if (rc != LEDGER89_OK)
        {
            return LEDGER89_EIO;
        }
    }
    rc = l->io->pread(l->io->ctx, fd, crcbuf, sizeof crcbuf,
                      offset + (led89_u64)LED89_RECORD_LENGTH_SIZE +
                          (led89_u64)len);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    crc = led89_crc32c(0u, lenbuf, sizeof lenbuf);
    if (len > 0u)
    {
        crc = led89_crc32c(crc, data_out, (size_t)len);
    }
    stored = led89_get_u32(crcbuf);
    if (crc != stored)
    {
        return LEDGER89_ECORRUPT;
    }
    return LEDGER89_OK;
}

static int led89_read_body(ledger89 *l, led89_fd fd,
                           const led89_batch_dir_entry *e, led89_u64 index,
                           void *data_out, size_t capacity, size_t *size_out)
{
    led89_u64 offset;
    int rc;

    rc = led89_body_offset(l, fd, e, index, &offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_body_read(l, fd, e, offset, data_out, capacity, size_out);
    return rc;
}

int led89_batch_read_record(ledger89 *l, const led89_batch_dir_entry *e,
                            led89_u64 index, void *data_out, size_t capacity,
                            size_t *size_out)
{
    led89_fd fd;
    int opened;
    int rc;

    rc = led89_part_fd(l, e->part, &fd, &opened);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_read_body(l, fd, e, index, data_out, capacity, size_out);
    if (opened != 0)
    {
        led89_close_quiet(l, fd);
    }
    return rc;
}

int led89_read_impl(ledger89 *l, led89_u64 index, void *data_out,
                    size_t capacity, size_t *size_out)
{
    size_t entry;
    int rc;

    if (index < l->first)
    {
        return LEDGER89_EGONE;
    }
    if (index >= l->end)
    {
        return LEDGER89_ENOENT;
    }
    rc = led89_dir_find(l, index, &entry);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ECORRUPT;
    }
    rc = led89_batch_read_record(l, &l->dir[entry], index, data_out, capacity,
                                 size_out);
    return rc;
}

/*
 * Return 1 when the iterator's cached position still addresses `index`,
 * filling the batch entry and physical offset. Return 0 when the cache must
 * be rebuilt from the batch directory.
 */
static int led89_cursor_match(ledger89 *l, const ledger89_iter *iter,
                              led89_u64 index, size_t *entry, led89_u64 *offset)
{
    const led89_batch_dir_entry *e;

    if (iter->cursor_valid == 0)
    {
        return 0;
    }
    if (iter->cursor_entry >= l->dir_count)
    {
        return 0;
    }
    e = &l->dir[iter->cursor_entry];
    if (index < e->first)
    {
        return 0;
    }
    if (index >= e->first + e->count)
    {
        return 0;
    }
    *entry = iter->cursor_entry;
    *offset = led89_from_public(iter->cursor_offset);
    return 1;
}

/* Physical offset of the first record of a batch, or zero when `index` is
 * not that record. */
static led89_u64 led89_entry_start(const led89_batch_dir_entry *e,
                                   led89_u64 index)
{
    if (index != e->first)
    {
        return (led89_u64)0;
    }
    return led89_first_off(e);
}

/* Resolve `index` to a batch entry and physical offset, preferring the
 * iterator cache. have_offset is 0 when the offset still needs a scan. */
static int led89_iter_locate(ledger89 *l, const ledger89_iter *iter,
                             led89_u64 index, size_t *entry, led89_u64 *offset,
                             int *have_offset)
{
    int matched;
    int rc;

    matched = led89_cursor_match(l, iter, index, entry, offset);
    if (matched != 0)
    {
        *have_offset = 1;
        return LEDGER89_OK;
    }
    rc = led89_dir_find(l, index, entry);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ECORRUPT;
    }
    *offset = led89_entry_start(&l->dir[*entry], index);
    *have_offset = 0;
    if (*offset != (led89_u64)0)
    {
        *have_offset = 1;
    }
    return LEDGER89_OK;
}

static void led89_cursor_clear(ledger89_iter *iter)
{
    iter->cursor_valid = 0;
}

static void led89_cursor_set(ledger89_iter *iter, size_t entry,
                             led89_u64 offset)
{
    iter->cursor_entry = entry;
    iter->cursor_offset = led89_to_public(offset);
    iter->cursor_valid = 1;
}

static void led89_iter_advance(ledger89_iter *iter,
                               const led89_batch_dir_entry *e, size_t entry,
                               led89_u64 offset, size_t size)
{
    led89_u64 next;
    led89_u64 end;

    next = led89_next_off(offset, size);
    end = e->offset + e->bytes;
    if (next >= end)
    {
        led89_cursor_clear(iter);
        return;
    }
    led89_cursor_set(iter, entry, next);
}

/*
 * Read the record at `index` for an iterator, reusing the iterator's cached
 * batch entry and physical record offset when they still address `index`.
 * Sequential iteration therefore advances linearly instead of rescanning
 * each batch from its start.
 */
int led89_iter_step(ledger89_iter *iter, led89_u64 index, void *data_out,
                    size_t capacity, size_t *size_out)
{
    ledger89 *l;
    const led89_batch_dir_entry *e;
    size_t entry;
    led89_u64 offset;
    led89_fd fd;
    int opened;
    int have_offset;
    int rc;

    l = iter->ledger;
    entry = 0u;
    offset = (led89_u64)0;
    have_offset = 0;
    rc = led89_iter_locate(l, iter, index, &entry, &offset, &have_offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    e = &l->dir[entry];
    rc = led89_part_fd(l, e->part, &fd, &opened);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (have_offset == 0)
    {
        rc = led89_body_offset(l, fd, e, index, &offset);
    }
    if (rc == LEDGER89_OK)
    {
        rc = led89_body_read(l, fd, e, offset, data_out, capacity, size_out);
    }
    if (rc == LEDGER89_OK)
    {
        led89_iter_advance(iter, e, entry, offset, *size_out);
    }
    else if (rc == LEDGER89_ETOOSMALL)
    {
        led89_cursor_set(iter, entry, offset);
    }
    if (opened != 0)
    {
        led89_close_quiet(l, fd);
    }
    return rc;
}
