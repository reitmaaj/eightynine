/* ledger89_segment.c - sequential record walker and active-segment append.
 *
 * The walker validates record CRCs and batch framing as it advances; it
 * yields payload pointers into its own reusable buffer. The append path
 * writes one atomic batch at the current active offset. */

#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

void led89_walk_init(led89_walk *w, const led89_io *io, led89_fd fd,
                     led89_u64 start, led89_u64 end)
{
    w->io = io;
    w->fd = fd;
    w->end = end;
    w->batch_start = start;
    w->batch_bytes = 0u;
    w->next_index = 0u;
    w->offset = start;
    w->batch_count = 0u;
    w->records_left = 0u;
    w->batch_crc = 0u;
    w->batch_active = 0;
    w->buf = NULL;
    w->cap = 0u;
    w->payload_size = 0u;
}

void led89_walk_restart(led89_walk *w, const led89_io *io, led89_fd fd,
                        led89_u64 start, led89_u64 end)
{
    unsigned char *buf;
    size_t cap;

    buf = w->buf;
    cap = w->cap;
    led89_walk_init(w, io, fd, start, end);
    w->buf = buf;
    w->cap = cap;
}

void led89_walk_free(led89_walk *w)
{
    free(w->buf);
    w->buf = NULL;
    w->cap = 0u;
}

static int led89_walk_ensure(led89_walk *w, size_t need)
{
    unsigned char *p;

    if (need <= w->cap)
    {
        return LEDGER89_OK;
    }
    p = (unsigned char *)realloc(w->buf, need);
    if (p == NULL)
    {
        return LEDGER89_ERR_NOMEM;
    }
    w->buf = p;
    w->cap = need;
    return LEDGER89_OK;
}

static int led89_walk_begin(led89_walk *w)
{
    unsigned char hdr[LED89_BATCH_HEADER_SIZE];
    led89_batch_header bh;
    int rc;

    rc = w->io->pread(w->io->ctx, w->fd, hdr, sizeof hdr, w->batch_start);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_batch_header_decode(hdr, &bh);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (bh.batch_bytes > w->end - w->batch_start)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    w->batch_bytes = bh.batch_bytes;
    w->batch_count = bh.record_count;
    w->records_left = bh.record_count;
    w->next_index = bh.first_index;
    w->offset = w->batch_start + (led89_u64)LED89_BATCH_HEADER_SIZE;
    w->batch_crc = led89_crc32c(0u, hdr, sizeof hdr);
    w->batch_active = 1;
    return LEDGER89_OK;
}

static int led89_walk_finish(led89_walk *w)
{
    unsigned char foot[LED89_BATCH_FOOTER_SIZE];
    led89_batch_footer bf;
    led89_u64 at;
    led89_u32 crc;
    int rc;

    if (w->batch_bytes < (led89_u64)LED89_MIN_BATCH_BYTES)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    at = w->batch_start + w->batch_bytes - (led89_u64)LED89_BATCH_FOOTER_SIZE;
    rc = w->io->pread(w->io->ctx, w->fd, foot, sizeof foot, at);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_batch_footer_decode(foot, &bf);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (bf.record_count != w->batch_count)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (bf.last_index + 1u != w->next_index)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    crc = led89_crc32c(w->batch_crc, foot, 16u);
    if (crc != bf.crc)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    w->batch_start += w->batch_bytes;
    return LEDGER89_OK;
}

static int led89_walk_record(led89_walk *w, led89_rec_header *rh,
                             const unsigned char **payload)
{
    unsigned char hdr[LED89_RECORD_HEADER_SIZE];
    unsigned char crcbuf[LED89_RECORD_CRC_SIZE];
    led89_u32 crc;
    led89_u32 stored;
    size_t need;
    int rc;

    if (w->end - w->offset < (led89_u64)LED89_RECORD_HEADER_SIZE)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    rc = w->io->pread(w->io->ctx, w->fd, hdr, sizeof hdr, w->offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_rec_header_decode(hdr, rh);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (rh->index != w->next_index)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if ((led89_u64)rh->payload_size + (led89_u64)LED89_RECORD_CRC_SIZE >
        w->end - w->offset - (led89_u64)LED89_RECORD_HEADER_SIZE)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    need = (size_t)LED89_RECORD_HEADER_SIZE + (size_t)rh->payload_size +
           (size_t)LED89_RECORD_CRC_SIZE;
    rc = led89_walk_ensure(w, need);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    memcpy(w->buf, hdr, sizeof hdr);
    rc = w->io->pread(w->io->ctx, w->fd, w->buf + LED89_RECORD_HEADER_SIZE,
                      (size_t)rh->payload_size + (size_t)LED89_RECORD_CRC_SIZE,
                      w->offset + (led89_u64)LED89_RECORD_HEADER_SIZE);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    memcpy(crcbuf, w->buf + LED89_RECORD_HEADER_SIZE + rh->payload_size,
           sizeof crcbuf);
    crc = led89_crc32c(0u, w->buf, (size_t)LED89_RECORD_HEADER_SIZE);
    crc = led89_crc32c(crc, w->buf + LED89_RECORD_HEADER_SIZE,
                       (size_t)rh->payload_size);
    stored = led89_get_u32(crcbuf);
    if (crc != stored)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    w->batch_crc =
        led89_crc32c(w->batch_crc, w->buf, (size_t)LED89_RECORD_HEADER_SIZE);
    w->batch_crc = led89_crc32c(w->batch_crc, w->buf + LED89_RECORD_HEADER_SIZE,
                                (size_t)rh->payload_size);
    w->batch_crc = led89_crc32c(w->batch_crc, crcbuf, sizeof crcbuf);
    w->offset += (led89_u64)LED89_RECORD_HEADER_SIZE +
                 (led89_u64)rh->payload_size + (led89_u64)LED89_RECORD_CRC_SIZE;
    w->records_left -= 1u;
    w->next_index += 1u;
    w->payload_size = (size_t)rh->payload_size;
    *payload = w->buf + LED89_RECORD_HEADER_SIZE;
    return LEDGER89_OK;
}

int led89_walk_next(led89_walk *w, led89_rec_header *rh,
                    const unsigned char **payload)
{
    int rc;

    if (w->records_left == 0u)
    {
        if (w->batch_active != 0)
        {
            rc = led89_walk_finish(w);
            if (rc != LEDGER89_OK)
            {
                return rc;
            }
            w->batch_active = 0;
        }
        if (w->batch_start >= w->end)
        {
            return LEDGER89_END;
        }
        rc = led89_walk_begin(w);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    rc = led89_walk_record(w, rh, payload);
    return rc;
}

static int led89_write_payload(ledger89 *l, led89_fd fd, led89_u64 *offset,
                               const ledger89_record *r)
{
    int rc;

    rc = l->io->pwrite(l->io->ctx, fd, r->data, r->size, *offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *offset += (led89_u64)r->size;
    return LEDGER89_OK;
}

int led89_write_record(ledger89 *l, led89_fd fd, led89_u64 *offset,
                       led89_u32 *bcrc, const ledger89_record *r)
{
    unsigned char hdr[LED89_RECORD_HEADER_SIZE];
    unsigned char crcbuf[LED89_RECORD_CRC_SIZE];
    led89_rec_header rh;
    led89_u32 crc;
    int rc;

    rh.index = (led89_u64)r->index;
    rh.tag = (led89_u64)r->tag;
    rh.payload_size = (led89_u32)r->size;
    rh.flags = 0u;
    led89_rec_header_encode(hdr, &rh);
    crc = led89_crc32c(0u, hdr, sizeof hdr);
    crc = led89_crc32c(crc, r->data, r->size);
    led89_put_u32(crcbuf, crc);
    rc = l->io->pwrite(l->io->ctx, fd, hdr, sizeof hdr, *offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *offset += (led89_u64)sizeof hdr;
    if (r->size > 0u)
    {
        rc = led89_write_payload(l, fd, offset, r);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    rc = l->io->pwrite(l->io->ctx, fd, crcbuf, sizeof crcbuf, *offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *offset += (led89_u64)sizeof crcbuf;
    *bcrc = led89_crc32c(*bcrc, hdr, sizeof hdr);
    *bcrc = led89_crc32c(*bcrc, r->data, r->size);
    *bcrc = led89_crc32c(*bcrc, crcbuf, sizeof crcbuf);
    return LEDGER89_OK;
}

int led89_write_batch(ledger89 *l, led89_fd fd, led89_u64 *offset,
                      const ledger89_record *records, size_t count,
                      led89_u64 first_index, led89_u64 batch_bytes)
{
    unsigned char hdr[LED89_BATCH_HEADER_SIZE];
    unsigned char foot[LED89_BATCH_FOOTER_SIZE];
    led89_batch_header bh;
    led89_batch_footer bf;
    led89_u32 bcrc;
    size_t i;
    int rc;

    bh.record_count = (led89_u32)count;
    bh.first_index = first_index;
    bh.batch_bytes = batch_bytes;
    led89_batch_header_encode(hdr, &bh);
    bcrc = led89_crc32c(0u, hdr, sizeof hdr);
    rc = l->io->pwrite(l->io->ctx, fd, hdr, sizeof hdr, *offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *offset += (led89_u64)sizeof hdr;
    for (i = 0u; i < count; ++i)
    {
        rc = led89_write_record(l, fd, offset, &bcrc, &records[i]);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    bf.record_count = (led89_u32)count;
    bf.last_index = (led89_u64)records[count - 1u].index;
    bf.crc = 0u;
    led89_batch_footer_encode(foot, &bf);
    bcrc = led89_crc32c(bcrc, foot, 16u);
    led89_put_u32(foot + 16, bcrc);
    rc = l->io->pwrite(l->io->ctx, fd, foot, sizeof foot, *offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *offset += (led89_u64)sizeof foot;
    return LEDGER89_OK;
}

static led89_u64 led89_add_record_bytes(led89_u64 total, size_t size)
{
    return total + (led89_u64)LED89_RECORD_HEADER_SIZE +
           (led89_u64)LED89_RECORD_CRC_SIZE + (led89_u64)size;
}
led89_u64 led89_batch_bytes(const ledger89_record *records, size_t count)
{
    led89_u64 total;
    size_t i;

    total =
        (led89_u64)LED89_BATCH_HEADER_SIZE + (led89_u64)LED89_BATCH_FOOTER_SIZE;
    for (i = 0u; i < count; ++i)
    {
        total = led89_add_record_bytes(total, records[i].size);
    }
    return total;
}

int led89_active_append(ledger89 *l, const ledger89_record *records,
                        size_t count)
{
    led89_u64 first_index;
    led89_u64 batch_bytes;
    led89_u64 offset;
    int rc;

    first_index = l->last_index + 1u;
    batch_bytes = led89_batch_bytes(records, count);
    offset = l->active_offset;
    rc = led89_write_batch(l, l->active_fd, &offset, records, count,
                           first_index, batch_bytes);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->active_offset = offset;
    l->active_records += (led89_u64)count;
    l->last_index = first_index + (led89_u64)count - 1u;
    l->dirty = 1;
    return LEDGER89_OK;
}

int led89_should_rotate(const ledger89 *l, led89_u64 count, led89_u64 bytes)
{
    if (l->active_records == 0u)
    {
        return 0;
    }
    if (l->seg_max_records > 0u)
    {
        if (l->active_records + count > l->seg_max_records)
        {
            return 1;
        }
    }
    if (l->seg_max_bytes > 0u)
    {
        if (l->active_offset + bytes > l->seg_max_bytes)
        {
            return 1;
        }
    }
    return 0;
}

static int led89_digest_step(ledger89 *l, led89_fd fd, unsigned char *buf,
                             size_t cap, led89_u64 *at, led89_u64 *left,
                             led89_u32 *crc)
{
    size_t chunk;
    int rc;

    chunk = led89_chunk_size(cap, *left);
    rc = l->io->pread(l->io->ctx, fd, buf, chunk, *at);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *crc = led89_crc32c(*crc, buf, chunk);
    *at += (led89_u64)chunk;
    *left -= (led89_u64)chunk;
    return LEDGER89_OK;
}

int led89_digest_range(ledger89 *l, led89_fd fd, led89_u64 start, led89_u64 end,
                       led89_u32 *out)
{
    unsigned char buf[4096];
    led89_u64 at;
    led89_u64 left;
    led89_u32 crc;
    int rc;

    crc = 0u;
    at = start;
    left = end - start;
    while (left > 0u)
    {
        rc = led89_digest_step(l, fd, buf, sizeof buf, &at, &left, &crc);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    *out = crc;
    return LEDGER89_OK;
}

static int led89_segment_digest(ledger89 *l, led89_u32 *out)
{
    int rc;

    rc = led89_digest_range(l, l->active_fd,
                            (led89_u64)LED89_SEGMENT_HEADER_SIZE,
                            l->active_offset, out);
    return rc;
}

static int led89_rename_active(ledger89 *l, const char *name)
{
    int rc;

    rc =
        led89_path_join(l->scratch, l->scratch_cap, l->path, LED89_ACTIVE_NAME);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_path_join(l->scratch2, l->scratch2_cap, l->path, name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->rename(l->io->ctx, l->scratch, l->scratch2);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->sync_dir(l->io->ctx, l->path);
    return rc;
}

int led89_seal_active(ledger89 *l)
{
    unsigned char foot[LED89_SEGMENT_FOOTER_SIZE];
    char name[LED89_NAME_MAX];
    led89_seg_footer sf;
    led89_u64 first;
    led89_u64 last;
    int rc;

    rc = led89_segment_digest(l, &sf.segment_digest);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    first = l->active_first;
    last = l->last_index;
    sf.last_index = last;
    sf.record_count = l->active_records;
    sf.body_size = l->active_offset - (led89_u64)LED89_SEGMENT_HEADER_SIZE;
    led89_seg_footer_encode(foot, &sf);
    rc = l->io->pwrite(l->io->ctx, l->active_fd, foot, sizeof foot,
                       l->active_offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->sync(l->io->ctx, l->active_fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->close(l->io->ctx, l->active_fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->active_open = 0;
    l->active_fd = -1;
    led89_name_format(name, first);
    rc = led89_rename_active(l, name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_segments_add(l, name, first, last);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_active_create(l, last + 1u);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->active_first = last + 1u;
    l->active_offset = (led89_u64)LED89_SEGMENT_HEADER_SIZE;
    l->active_records = 0u;
    l->dirty = 0;
    return LEDGER89_OK;
}
