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

static int led89_read_body(ledger89 *l, led89_fd fd,
                           const led89_batch_dir_entry *e, led89_u64 index,
                           void *data_out, size_t capacity, size_t *size_out)
{
    unsigned char lenbuf[LED89_RECORD_LENGTH_SIZE];
    unsigned char crcbuf[LED89_RECORD_CRC_SIZE];
    led89_u64 end;
    led89_u64 offset;
    led89_u64 skip;
    led89_u32 len;
    led89_u32 stored;
    led89_u32 crc;
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
