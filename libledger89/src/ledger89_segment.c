/* ledger89_segment.c - part files: creation, scanning, append batches,
 * stable markers, sealing, and rotation. */
#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

int led89_part_open(ledger89 *l, led89_u64 file_id, int flags, led89_fd *fd)
{
    char name[LED89_NAME_MAX];
    int rc;

    led89_part_name(name, file_id);
    rc = led89_path_join(l->scratch, l->scratch_cap, l->path, name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->open(l->io->ctx, l->scratch, flags, fd);
    return rc;
}

static void led89_part_clear(ledger89 *l, led89_part *p)
{
    led89_close_quiet(l, p->fd);
    p->open = 0;
    p->fd = -1;
}

void led89_part_close(ledger89 *l, led89_part *p)
{
    if (p->open != 0)
    {
        led89_part_clear(l, p);
    }
}

int led89_dir_add(ledger89 *l, size_t part, led89_u64 first, led89_u64 count,
                  led89_u64 offset, led89_u64 bytes)
{
    led89_batch_dir_entry *e;
    int rc;

    rc = led89_handle_reserve_dir(l, l->dir_count + 1u);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    e = &l->dir[l->dir_count];
    e->first = first;
    e->count = count;
    e->offset = offset;
    e->bytes = bytes;
    e->part = part;
    ++l->dir_count;
    return LEDGER89_OK;
}

int led89_write_marker(ledger89 *l, led89_fd fd, led89_u64 file_id,
                       led89_u64 revision, led89_u64 *offset, led89_u64 end)
{
    led89_marker m;
    unsigned char buf[LED89_MARKER_SIZE];
    int rc;

    memcpy(m.uuid, l->id.bytes, 16u);
    m.file_id = file_id;
    m.revision = revision;
    m.end = end;
    led89_marker_encode(buf, &m);
    rc = l->io->pwrite(l->io->ctx, fd, buf, sizeof buf, *offset);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    *offset += (led89_u64)LED89_MARKER_SIZE;
    return LEDGER89_OK;
}

static int led89_write_part_header(ledger89 *l, led89_fd fd, led89_u64 file_id,
                                   led89_u64 revision, led89_u64 first)
{
    led89_part_header h;
    int rc;

    rc = led89_buf_reserve(l, LED89_PART_HEADER_SIZE);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    memcpy(h.uuid, l->id.bytes, 16u);
    h.file_id = file_id;
    h.revision = revision;
    h.first = first;
    led89_part_header_encode(l->buf, &h);
    rc = l->io->pwrite(l->io->ctx, fd, l->buf, (size_t)LED89_PART_HEADER_SIZE,
                       (led89_u64)0);
    return rc;
}

int led89_part_create(ledger89 *l, led89_u64 file_id, led89_u64 first,
                      led89_u64 revision, size_t *index_out)
{
    led89_part *p;
    led89_fd fd;
    led89_u64 off;
    int rc;

    rc = led89_part_open(l, file_id, LED89_OPEN_WRITE | LED89_OPEN_CREATE, &fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->truncate(l->io->ctx, fd, (led89_u64)0);
    if (rc == LEDGER89_OK)
    {
        rc = led89_write_part_header(l, fd, file_id, revision, first);
    }
    off = (led89_u64)LED89_PART_HEADER_SIZE;
    if (rc == LEDGER89_OK)
    {
        rc = led89_write_marker(l, fd, file_id, revision, &off, first);
    }
    if (rc == LEDGER89_OK)
    {
        rc = l->io->sync(l->io->ctx, fd);
    }
    if (rc != LEDGER89_OK)
    {
        led89_close_quiet(l, fd);
        return rc;
    }
    rc = led89_handle_reserve_parts(l, l->part_count + 1u);
    if (rc != LEDGER89_OK)
    {
        led89_close_quiet(l, fd);
        return rc;
    }
    p = &l->parts[l->part_count];
    p->desc.file_id = file_id;
    p->desc.first = first;
    p->desc.end = first;
    p->sealed = 0;
    p->open = 1;
    p->fd = fd;
    p->bytes = off;
    *index_out = l->part_count;
    ++l->part_count;
    return LEDGER89_OK;
}

static led89_u64 led89_slice_size(const ledger89_slice *r)
{
    return (led89_u64)r->size;
}

static led89_u64 led89_record_bytes(led89_u64 size)
{
    return size + (led89_u64)(LED89_RECORD_LENGTH_SIZE + LED89_RECORD_CRC_SIZE);
}

int led89_batch_total(const ledger89_slice *records, size_t count,
                      led89_u64 *out)
{
    led89_u64 total;
    size_t i;

    total = (led89_u64)LED89_MIN_BATCH_BYTES;
    for (i = 0u; i < count; ++i)
    {
        led89_u64 add;

        add = led89_record_bytes(led89_slice_size(&records[i]));
        if (led89_u64_add(total, add, &total) == 0)
        {
            return LEDGER89_EOVERFLOW;
        }
    }
    *out = total;
    return LEDGER89_OK;
}

static int led89_emit_record(ledger89 *l, led89_fd fd, unsigned char *lenbuf,
                             unsigned char *crcbuf, led89_u64 *at,
                             const ledger89_slice *r, led89_u32 *crc)
{
    led89_u32 payload_crc;
    int rc;

    led89_put_u32(lenbuf, (led89_u32)r->size);
    payload_crc = led89_crc32c(0u, lenbuf, (size_t)LED89_RECORD_LENGTH_SIZE);
    if (r->size > 0u)
    {
        payload_crc = led89_crc32c(payload_crc, r->data, r->size);
    }
    led89_put_u32(crcbuf, payload_crc);
    rc = l->io->pwrite(l->io->ctx, fd, lenbuf, (size_t)LED89_RECORD_LENGTH_SIZE,
                       *at);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    *at = led89_at_add(*at, (led89_u64)LED89_RECORD_LENGTH_SIZE);
    if (r->size > 0u)
    {
        rc = l->io->pwrite(l->io->ctx, fd, r->data, r->size, *at);
        if (rc != LEDGER89_OK)
        {
            return LEDGER89_EIO;
        }
    }
    *at = led89_at_add(*at, (led89_u64)r->size);
    rc = l->io->pwrite(l->io->ctx, fd, crcbuf, (size_t)LED89_RECORD_CRC_SIZE,
                       *at);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    *at = led89_at_add(*at, (led89_u64)LED89_RECORD_CRC_SIZE);
    *crc = led89_crc32c(*crc, lenbuf, (size_t)LED89_RECORD_LENGTH_SIZE);
    if (r->size > 0u)
    {
        *crc = led89_crc32c(*crc, r->data, r->size);
    }
    *crc = led89_crc32c(*crc, crcbuf, (size_t)LED89_RECORD_CRC_SIZE);
    return LEDGER89_OK;
}

int led89_emit_batch(ledger89 *l, led89_fd fd, led89_u64 *offset,
                     const ledger89_slice *records, size_t count,
                     led89_u64 first)
{
    led89_batch_header h;
    led89_batch_footer f;
    unsigned char hdr[LED89_BATCH_HEADER_SIZE];
    unsigned char ftr[LED89_BATCH_FOOTER_SIZE];
    unsigned char lenbuf[LED89_RECORD_LENGTH_SIZE];
    unsigned char crcbuf[LED89_RECORD_CRC_SIZE];
    led89_u64 total;
    led89_u64 at;
    led89_u32 crc;
    size_t i;
    int rc;

    rc = led89_batch_total(records, count, &total);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if ((led89_u64)count > (led89_u64)0xFFFFFFFFu)
    {
        return LEDGER89_ERANGE;
    }
    h.count = (led89_u32)count;
    h.first = first;
    h.bytes = total;
    led89_batch_header_encode(hdr, &h);
    at = *offset;
    rc = l->io->pwrite(l->io->ctx, fd, hdr, sizeof hdr, at);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    at += (led89_u64)LED89_BATCH_HEADER_SIZE;
    crc = led89_crc32c(0u, hdr, sizeof hdr);
    for (i = 0u; i < count; ++i)
    {
        rc = led89_emit_record(l, fd, lenbuf, crcbuf, &at, &records[i], &crc);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    f.count = (led89_u32)count;
    f.last = first + (led89_u64)count - (led89_u64)1;
    led89_batch_footer_encode(ftr, &f);
    crc = led89_crc32c(crc, ftr, 16u);
    led89_put_u32(ftr + 16, crc);
    rc = l->io->pwrite(l->io->ctx, fd, ftr, sizeof ftr, at);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    *offset = *offset + total;
    return LEDGER89_OK;
}

int led89_active_append(ledger89 *l, const ledger89_slice *records,
                        size_t count, led89_u64 *first_out)
{
    led89_part *p;
    led89_u64 first;
    led89_u64 total;
    led89_u64 off;
    int rc;

    p = &l->parts[l->active_index];
    first = l->end;
    rc = led89_batch_total(records, count, &total);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    off = p->bytes;
    rc = led89_emit_batch(l, p->fd, &off, records, count, first);
    if (rc != LEDGER89_OK)
    {
        rc = led89_io_error(l, rc);
        return rc;
    }
    rc = led89_dir_add(l, l->active_index, first, (led89_u64)count, p->bytes,
                       total);
    if (rc != LEDGER89_OK)
    {
        l->poisoned = 1;
        return rc;
    }
    p->bytes = off;
    l->end = first + (led89_u64)count;
    l->dirty = 1;
    if (first_out != NULL)
    {
        *first_out = first;
    }
    return LEDGER89_OK;
}

static int led89_digest_step(ledger89 *l, led89_fd fd, led89_u64 end,
                             led89_u64 *off, led89_u32 *crc)
{
    size_t chunk;
    int rc;

    chunk = led89_chunk_of(led89_u64_left(end, *off));
    rc = led89_buf_reserve(l, chunk);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->pread(l->io->ctx, fd, l->buf, chunk, *off);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    *crc = led89_crc32c(*crc, l->buf, chunk);
    *off = led89_at_add(*off, (led89_u64)chunk);
    return LEDGER89_OK;
}

static int led89_digest_range(ledger89 *l, led89_fd fd, led89_u64 start,
                              led89_u64 end, led89_u32 *out)
{
    led89_u64 off;
    led89_u32 crc;
    int rc;

    crc = 0u;
    off = start;
    while (off < end)
    {
        rc = led89_digest_step(l, fd, end, &off, &crc);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    *out = crc;
    return LEDGER89_OK;
}

int led89_write_sealed_footer(ledger89 *l, led89_part *p, led89_u64 end)
{
    led89_sealed_footer f;
    unsigned char buf[LED89_SEALED_FOOTER_SIZE];
    led89_u32 digest;
    int rc;

    rc = led89_digest_range(l, p->fd, (led89_u64)LED89_PART_HEADER_SIZE,
                            p->bytes, &digest);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    memcpy(f.uuid, l->id.bytes, 16u);
    f.file_id = p->desc.file_id;
    f.first = p->desc.first;
    f.end = end;
    f.records = end - p->desc.first;
    f.digest = digest;
    led89_sealed_footer_encode(buf, &f);
    rc = l->io->pwrite(l->io->ctx, p->fd, buf, sizeof buf, p->bytes);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    rc = l->io->sync(l->io->ctx, p->fd);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    p->bytes += (led89_u64)LED89_SEALED_FOOTER_SIZE;
    p->desc.end = end;
    return LEDGER89_OK;
}

int led89_sync_impl(ledger89 *l)
{
    led89_part *p;
    led89_u64 off;
    int rc;

    if (l->dirty == 0)
    {
        return LEDGER89_OK;
    }
    p = &l->parts[l->active_index];
    off = p->bytes;
    rc = led89_write_marker(l, p->fd, p->desc.file_id, l->revision, &off,
                            l->end);
    if (rc == LEDGER89_OK)
    {
        rc = l->io->sync(l->io->ctx, p->fd);
    }
    if (rc != LEDGER89_OK)
    {
        rc = led89_io_error(l, rc);
        return rc;
    }
    p->bytes = off;
    l->stable_end = l->end;
    l->dirty = 0;
    return LEDGER89_OK;
}

int led89_seal_active(ledger89 *l)
{
    led89_part *p;
    int rc;

    p = &l->parts[l->active_index];
    rc = led89_write_sealed_footer(l, p, l->end);
    if (rc != LEDGER89_OK)
    {
        rc = led89_io_error(l, rc);
        return rc;
    }
    p->sealed = 1;
    led89_part_close(l, p);
    return LEDGER89_OK;
}

static void led89_copy_desc(led89_part_desc *dst, const led89_part_desc *src)
{
    *dst = *src;
}

int led89_rotate_impl(ledger89 *l)
{
    led89_part *old;
    led89_manifest m;
    size_t new_index;
    size_t i;
    led89_u64 new_id;
    int rc;

    old = &l->parts[l->active_index];
    if (l->end == old->desc.first)
    {
        return LEDGER89_OK;
    }
    rc = led89_sync_impl(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_seal_active(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    new_id = l->next_file_id;
    rc = led89_part_create(l, new_id, l->end, l->revision, &new_index);
    if (rc != LEDGER89_OK)
    {
        rc = led89_io_error(l, rc);
        return rc;
    }
    l->next_file_id = led89_u64_inc(l->next_file_id);
    m.generation = led89_u64_inc(l->generation);
    memcpy(m.uuid, l->id.bytes, 16u);
    m.revision = l->revision;
    m.first = l->first;
    m.sealed_count = (led89_u32)(l->part_count - 1u);
    m.sealed = (led89_part_desc *)malloc((size_t)m.sealed_count *
                                         sizeof(led89_part_desc));
    if (m.sealed == NULL)
    {
        rc = led89_io_error(l, LEDGER89_ENOMEM);
        return rc;
    }
    for (i = 0u; i + 1u < l->part_count; ++i)
    {
        led89_copy_desc(&m.sealed[i], &l->parts[i].desc);
    }
    m.active = l->parts[new_index].desc;
    rc = led89_manifest_publish(l, &m);
    if (rc == LEDGER89_OK)
    {
        rc = led89_current_publish(l, m.generation);
    }
    free(m.sealed);
    if (rc != LEDGER89_OK)
    {
        rc = led89_io_error(l, rc);
        return rc;
    }
    l->generation = m.generation;
    l->active_index = new_index;
    l->dirty = 0;
    return LEDGER89_OK;
}
