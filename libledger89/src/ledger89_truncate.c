/* ledger89_truncate.c - copy-on-write suffix truncation and prefix pruning.
 *
 * Files referenced by the current manifest are never destructively modified.
 * A structural transition prepares new files, publishes a new manifest, then
 * atomically replaces CURRENT. A crash resolves to the old topology or the
 * new topology, never a mixture. */
#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

static void led89_unlink_part(ledger89 *l, led89_u64 file_id)
{
    char name[LED89_NAME_MAX];
    int rc;

    led89_part_name(name, file_id);
    rc = led89_path_join(l->scratch, l->scratch_cap, l->path, name);
    if (rc == LEDGER89_OK)
    {
        led89_unlink_quiet(l, l->scratch);
    }
}

static led89_u64 led89_t_desc_id(const led89_part_desc *d)
{
    return d->file_id;
}

static void led89_t_copy_desc(led89_part_desc *dst, const led89_part_desc *src)
{
    *dst = *src;
}

static void led89_t_copy_part(led89_part *dst, const led89_part *src)
{
    *dst = *src;
}

static size_t led89_t_inc(size_t v)
{
    return v + 1u;
}

static const void *led89_t_slice_data(ledger89 *l, size_t size)
{
    if (size > 0u)
    {
        return l->buf;
    }
    return NULL;
}

static void led89_t_slice_set(ledger89_slice *s, const void *data, size_t size)
{
    s->data = data;
    s->size = size;
}

static led89_part_desc *led89_t_alloc_descs(size_t count)
{
    led89_part_desc *p;

    p = (led89_part_desc *)malloc(count * sizeof(led89_part_desc));
    return p;
}

static led89_u64 *led89_t_alloc_ids(size_t count)
{
    led89_u64 *p;

    p = (led89_u64 *)malloc(count * sizeof(led89_u64));
    return p;
}

static int led89_copy_step(ledger89 *l, led89_fd src, led89_u64 *src_off,
                           led89_fd dst, led89_u64 *dst_off, led89_u64 *left)
{
    size_t chunk;
    int rc;

    chunk = led89_chunk_of(*left);
    rc = led89_buf_reserve(l, chunk);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->pread(l->io->ctx, src, l->buf, chunk, *src_off);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    rc = l->io->pwrite(l->io->ctx, dst, l->buf, chunk, *dst_off);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    *src_off = led89_at_add(*src_off, (led89_u64)chunk);
    *dst_off = led89_at_add(*dst_off, (led89_u64)chunk);
    *left = led89_u64_left(*left, (led89_u64)chunk);
    return LEDGER89_OK;
}

static int led89_copy_range(ledger89 *l, led89_fd src, led89_u64 src_off,
                            led89_u64 len, led89_fd dst, led89_u64 *dst_off)
{
    led89_u64 left;
    int rc;

    left = len;
    while (left > (led89_u64)0)
    {
        rc = led89_copy_step(l, src, &src_off, dst, dst_off, &left);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    return LEDGER89_OK;
}

static int led89_copy_one_record(ledger89 *l, size_t src_entry, size_t dst_part,
                                 led89_u64 index, led89_u64 *dst_off)
{
    ledger89_slice slice;
    led89_u64 start;
    size_t size;
    int rc;

    rc = led89_batch_read_record(l, &l->dir[src_entry], index, NULL, 0u, &size);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_buf_reserve(l, size);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_batch_read_record(l, &l->dir[src_entry], index, l->buf, size,
                                 &size);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    led89_t_slice_set(&slice, led89_t_slice_data(l, size), size);
    start = *dst_off;
    rc = led89_emit_batch(l, l->parts[dst_part].fd, dst_off, &slice, 1u, index);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_dir_add(l, dst_part, index, (led89_u64)1, start,
                       *dst_off - start);
    return rc;
}

static int led89_copy_batch(ledger89 *l, size_t i, led89_fd src_fd,
                            size_t dst_index, led89_u64 *dst_off)
{
    led89_u64 first;
    led89_u64 count;
    led89_u64 offset;
    led89_u64 bytes;
    led89_u64 start;
    int rc;

    first = l->dir[i].first;
    count = l->dir[i].count;
    offset = l->dir[i].offset;
    bytes = l->dir[i].bytes;
    start = *dst_off;
    rc = led89_copy_range(l, src_fd, offset, bytes, l->parts[dst_index].fd,
                          dst_off);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_dir_add(l, dst_index, first, count, start, bytes);
    return rc;
}

static int led89_copy_split(ledger89 *l, size_t i, led89_u64 first,
                            led89_u64 keep, size_t dst_index,
                            led89_u64 *dst_off)
{
    led89_u64 k;
    int rc;

    rc = LEDGER89_OK;
    for (k = (led89_u64)0; k < keep; ++k)
    {
        rc = led89_copy_one_record(l, i, dst_index, first + k, dst_off);
        if (rc != LEDGER89_OK)
        {
            break;
        }
    }
    return rc;
}

static int led89_copy_entry(ledger89 *l, size_t i, led89_u64 from,
                            led89_fd src_fd, size_t dst_index,
                            led89_u64 *dst_off, int *done)
{
    led89_u64 first;
    led89_u64 count;
    led89_u64 keep;
    int rc;

    first = l->dir[i].first;
    count = l->dir[i].count;
    *done = 0;
    if (first >= from)
    {
        *done = 1;
        return LEDGER89_OK;
    }
    keep = count;
    if (first + count > from)
    {
        keep = led89_u64_left(from, first);
    }
    if (keep == count)
    {
        rc = led89_copy_batch(l, i, src_fd, dst_index, dst_off);
    }
    else
    {
        rc = led89_copy_split(l, i, first, keep, dst_index, dst_off);
    }
    return rc;
}

static int led89_copy_prefix(ledger89 *l, size_t src_index, led89_u64 from,
                             size_t dst_index, led89_u64 *dst_off)
{
    led89_fd src_fd;
    size_t i;
    int rc;

    rc = led89_part_open(l, l->parts[src_index].desc.file_id, LED89_OPEN_READ,
                         &src_fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = LEDGER89_OK;
    i = 0u;
    for (;;)
    {
        if (i >= l->dir_count)
        {
            break;
        }
        if (l->dir[i].part == src_index)
        {
            break;
        }
        ++i;
    }
    for (;;)
    {
        int done;

        if (i >= l->dir_count)
        {
            break;
        }
        if (l->dir[i].part != src_index)
        {
            break;
        }
        rc = led89_copy_entry(l, i, from, src_fd, dst_index, dst_off, &done);
        if (rc != LEDGER89_OK)
        {
            break;
        }
        if (done != 0)
        {
            break;
        }
        ++i;
    }
    led89_close_quiet(l, src_fd);
    return rc;
}

static int led89_publish_manifest(ledger89 *l, led89_u64 revision,
                                  led89_u64 first, size_t sealed_count)
{
    led89_manifest m;
    size_t i;
    int rc;

    m.generation = led89_u64_inc(l->generation);
    memcpy(m.uuid, l->id.bytes, 16u);
    m.revision = revision;
    m.first = first;
    m.sealed_count = (led89_u32)sealed_count;
    m.sealed = NULL;
    if (sealed_count > 0u)
    {
        m.sealed = led89_t_alloc_descs(sealed_count);
        if (m.sealed == NULL)
        {
            return LEDGER89_ENOMEM;
        }
        for (i = 0u; i < sealed_count; ++i)
        {
            led89_t_copy_desc(&m.sealed[i], &l->parts[i].desc);
        }
    }
    led89_t_copy_desc(&m.active, &l->parts[sealed_count].desc);
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
    return LEDGER89_OK;
}

static size_t led89_count_keep(const ledger89 *l, size_t sealed_count,
                               led89_u64 from)
{
    size_t keep;

    keep = 0u;
    for (;;)
    {
        if (keep >= sealed_count)
        {
            break;
        }
        if (l->parts[keep].desc.end > from)
        {
            break;
        }
        keep = led89_t_inc(keep);
    }
    return keep;
}

static size_t led89_kept_entries(const ledger89 *l, size_t keep_sealed)
{
    size_t n;

    n = 0u;
    for (;;)
    {
        if (n >= l->dir_count)
        {
            break;
        }
        if (l->dir[n].part >= keep_sealed)
        {
            break;
        }
        n = led89_t_inc(n);
    }
    return n;
}

static int led89_make_copy(ledger89 *l, size_t boundary, led89_u64 from,
                           led89_u64 new_revision, size_t *copy_index)
{
    led89_u64 off;
    int rc;

    rc = led89_part_create(l, l->next_file_id, l->parts[boundary].desc.first,
                           new_revision, copy_index);
    if (rc != LEDGER89_OK)
    {
        rc = led89_io_error(l, rc);
        return rc;
    }
    l->next_file_id = led89_u64_inc(l->next_file_id);
    off = l->parts[*copy_index].bytes;
    rc = led89_copy_prefix(l, boundary, from, *copy_index, &off);
    if (rc == LEDGER89_OK)
    {
        rc = led89_write_marker(l, l->parts[*copy_index].fd,
                                l->parts[*copy_index].desc.file_id,
                                new_revision, &off, from);
    }
    l->parts[*copy_index].bytes = off;
    if (rc == LEDGER89_OK)
    {
        rc = led89_write_sealed_footer(l, &l->parts[*copy_index], from);
    }
    if (rc != LEDGER89_OK)
    {
        rc = led89_io_error(l, rc);
        return rc;
    }
    l->parts[*copy_index].sealed = 1;
    led89_part_close(l, &l->parts[*copy_index]);
    return LEDGER89_OK;
}

static void led89_move_dir(ledger89 *l, size_t keep_entries,
                           size_t old_dir_count, size_t keep_sealed)
{
    size_t copy_entries;
    size_t i;

    copy_entries = l->dir_count - old_dir_count;
    memmove(&l->dir[keep_entries], &l->dir[old_dir_count],
            copy_entries * sizeof(led89_batch_dir_entry));
    l->dir_count = keep_entries + copy_entries;
    for (i = keep_entries; i < l->dir_count; ++i)
    {
        l->dir[i].part = keep_sealed;
    }
}

static size_t led89_take_obsolete(ledger89 *l, size_t i, led89_u64 *dst,
                                  size_t n)
{
    dst[n] = led89_t_desc_id(&l->parts[i].desc);
    led89_part_close(l, &l->parts[i]);
    return led89_t_inc(n);
}

static size_t led89_rebuild_step(ledger89 *l, size_t pos, size_t copy_index)
{
    led89_t_copy_part(&l->parts[pos], &l->parts[copy_index]);
    return led89_t_inc(pos);
}

static void led89_rebuild_parts(ledger89 *l, size_t keep_sealed, int need_copy,
                                size_t copy_index, size_t new_active)
{
    size_t pos;

    pos = keep_sealed;
    if (need_copy != 0)
    {
        pos = led89_rebuild_step(l, pos, copy_index);
    }
    led89_t_copy_part(&l->parts[pos], &l->parts[new_active]);
    l->part_count = led89_t_inc(pos);
    l->active_index = pos;
}

int led89_truncate_impl(ledger89 *l, led89_u64 from)
{
    size_t old_count;
    size_t sealed_count;
    size_t keep_sealed;
    size_t boundary;
    size_t old_dir_count;
    size_t keep_entries;
    size_t copy_index;
    size_t new_active;
    size_t obsolete_count;
    led89_u64 new_revision;
    led89_u64 *obsolete;
    size_t i;
    int need_copy;
    int rc;

    if (from == l->end)
    {
        return LEDGER89_OK;
    }
    old_count = l->part_count;
    sealed_count = old_count - 1u;
    keep_sealed = led89_count_keep(l, sealed_count, from);
    boundary = keep_sealed;
    need_copy = 0;
    if (from > l->parts[boundary].desc.first)
    {
        need_copy = 1;
    }
    old_dir_count = l->dir_count;
    keep_entries = led89_kept_entries(l, keep_sealed);
    new_revision = led89_u64_inc(l->revision);
    copy_index = 0u;
    if (need_copy != 0)
    {
        rc = led89_make_copy(l, boundary, from, new_revision, &copy_index);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    rc = led89_part_create(l, l->next_file_id, from, new_revision, &new_active);
    if (rc != LEDGER89_OK)
    {
        rc = led89_io_error(l, rc);
        return rc;
    }
    l->next_file_id = led89_u64_inc(l->next_file_id);
    if (need_copy != 0)
    {
        led89_move_dir(l, keep_entries, old_dir_count, keep_sealed);
    }
    else
    {
        l->dir_count = keep_entries;
    }
    obsolete = led89_t_alloc_ids(old_count);
    if (obsolete == NULL)
    {
        rc = led89_io_error(l, LEDGER89_ENOMEM);
        return rc;
    }
    obsolete_count = 0u;
    for (i = keep_sealed; i < old_count; ++i)
    {
        obsolete_count = led89_take_obsolete(l, i, obsolete, obsolete_count);
    }
    led89_rebuild_parts(l, keep_sealed, need_copy, copy_index, new_active);
    rc = led89_publish_manifest(l, new_revision, l->first, l->part_count - 1u);
    if (rc != LEDGER89_OK)
    {
        free(obsolete);
        return rc;
    }
    l->revision = new_revision;
    l->end = from;
    l->stable_end = from;
    l->dirty = 0;
    for (i = 0u; i < obsolete_count; ++i)
    {
        led89_unlink_part(l, obsolete[i]);
    }
    led89_sync_dir_quiet(l);
    free(obsolete);
    return LEDGER89_OK;
}

static size_t led89_count_drop(const ledger89 *l, size_t sealed_count,
                               led89_u64 requested)
{
    size_t drop;

    drop = 0u;
    for (;;)
    {
        if (drop >= sealed_count)
        {
            break;
        }
        if (l->parts[drop].desc.end > requested)
        {
            break;
        }
        drop = led89_t_inc(drop);
    }
    return drop;
}

static size_t led89_count_drop_entries(const ledger89 *l, size_t drop)
{
    size_t n;

    n = 0u;
    for (;;)
    {
        if (n >= l->dir_count)
        {
            break;
        }
        if (l->dir[n].part >= drop)
        {
            break;
        }
        n = led89_t_inc(n);
    }
    return n;
}

static void led89_shift_parts(ledger89 *l, size_t drop)
{
    size_t i;

    for (i = drop; i < l->part_count; ++i)
    {
        led89_t_copy_part(&l->parts[i - drop], &l->parts[i]);
    }
}

static void led89_dir_rebase(led89_batch_dir_entry *e, size_t drop)
{
    e->part -= drop;
}

static void led89_shift_dir(ledger89 *l, size_t drop_entries, size_t drop)
{
    size_t i;

    memmove(&l->dir[0], &l->dir[drop_entries],
            (l->dir_count - drop_entries) * sizeof(led89_batch_dir_entry));
    l->dir_count -= drop_entries;
    for (i = 0u; i < l->dir_count; ++i)
    {
        led89_dir_rebase(&l->dir[i], drop);
    }
}

static void led89_set_actual(led89_u64 *out, led89_u64 v)
{
    *out = v;
}

int led89_prune_impl(ledger89 *l, led89_u64 requested, led89_u64 *actual)
{
    size_t sealed_count;
    size_t drop;
    size_t drop_entries;
    size_t obsolete_count;
    led89_u64 new_first;
    led89_u64 *obsolete;
    size_t i;
    int rc;

    if (requested <= l->first)
    {
        led89_set_actual(actual, l->first);
        return LEDGER89_OK;
    }
    sealed_count = l->part_count - 1u;
    drop = led89_count_drop(l, sealed_count, requested);
    if (drop == 0u)
    {
        led89_set_actual(actual, l->first);
        return LEDGER89_OK;
    }
    new_first = l->parts[drop].desc.first;
    drop_entries = led89_count_drop_entries(l, drop);
    obsolete = led89_t_alloc_ids(drop);
    if (obsolete == NULL)
    {
        return LEDGER89_ENOMEM;
    }
    obsolete_count = 0u;
    for (i = 0u; i < drop; ++i)
    {
        obsolete_count = led89_take_obsolete(l, i, obsolete, obsolete_count);
    }
    led89_shift_parts(l, drop);
    l->part_count -= drop;
    l->active_index = l->part_count - 1u;
    led89_shift_dir(l, drop_entries, drop);
    l->first = new_first;
    rc = led89_publish_manifest(l, l->revision, l->first, l->part_count - 1u);
    if (rc != LEDGER89_OK)
    {
        free(obsolete);
        return rc;
    }
    for (i = 0u; i < drop; ++i)
    {
        led89_unlink_part(l, obsolete[i]);
    }
    led89_sync_dir_quiet(l);
    free(obsolete);
    led89_set_actual(actual, new_first);
    return LEDGER89_OK;
}
