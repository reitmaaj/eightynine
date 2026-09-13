/* ledger89_truncate.c - suffix truncation and prefix discard.
 *
 * Both operations are self-durable and preserve a contiguous range at every
 * crash point. Segment files are unique keys; the header's first_index is
 * authoritative. A boundary segment is rewritten atomically through a temp
 * file; tail or front segments are removed one unlink plus directory sync at
 * a time so every intermediate durable state is a contiguous range. */

#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

/* --- small helpers ---------------------------------------------------- */

static void led89_tmp_close(ledger89 *l, led89_fd fd)
{
    l->io->close(l->io->ctx, fd);
}

int led89_install_tmp(ledger89 *l, const char *target_name)
{
    int rc;

    rc = led89_path_join(l->scratch, l->scratch_cap, l->path, target_name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_path_join(l->scratch2, l->scratch2_cap, l->path, LED89_TMP_NAME);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->rename(l->io->ctx, l->scratch2, l->scratch);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->sync_dir(l->io->ctx, l->path);
    return rc;
}

static int led89_open_sealed(ledger89 *l, size_t i, led89_fd *fd)
{
    int rc;

    rc = led89_path_join(l->scratch, l->scratch_cap, l->path,
                         l->segments[i].name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->open(l->io->ctx, l->scratch, LED89_OPEN_READ, fd);
    return rc;
}

static void led89_active_shutdown(ledger89 *l)
{
    l->io->close(l->io->ctx, l->active_fd);
    l->active_open = 0;
    l->active_fd = -1;
}

void led89_active_close(ledger89 *l)
{
    if (l->active_open != 0)
    {
        led89_active_shutdown(l);
    }
}

static int led89_unlink_active(ledger89 *l)
{
    int rc;

    rc = led89_active_path(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    led89_active_close(l);
    rc = l->io->unlink(l->io->ctx, l->scratch);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->sync_dir(l->io->ctx, l->path);
    return rc;
}

static int led89_unlink_sealed(ledger89 *l, size_t i)
{
    int rc;

    rc = led89_path_join(l->scratch, l->scratch_cap, l->path,
                         l->segments[i].name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->unlink(l->io->ctx, l->scratch);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->sync_dir(l->io->ctx, l->path);
    return rc;
}

static int led89_rename_sealed_active(ledger89 *l, size_t i)
{
    int rc;

    rc = led89_path_join(l->scratch, l->scratch_cap, l->path,
                         l->segments[i].name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_path_join(l->scratch2, l->scratch2_cap, l->path,
                         LED89_ACTIVE_NAME);
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

static int led89_active_reset(ledger89 *l, led89_u64 first)
{
    int rc;

    led89_active_close(l);
    rc = led89_active_create(l, first);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->active_first = first;
    l->active_offset = (led89_u64)LED89_SEGMENT_HEADER_SIZE;
    l->active_records = 0u;
    l->dirty = 0;
    return LEDGER89_OK;
}

static int led89_active_reopen(ledger89 *l)
{
    int rc;

    led89_active_close(l);
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

/* --- boundary rewrite ------------------------------------------------- */

static led89_u64 led89_body_size(led89_u64 offset)
{
    return offset - (led89_u64)LED89_SEGMENT_HEADER_SIZE;
}

static led89_u64 led89_add_footer(led89_u64 offset)
{
    return offset + (led89_u64)LED89_SEGMENT_FOOTER_SIZE;
}

static int led89_write_footer(ledger89 *l, led89_fd fd, led89_u64 offset,
                              led89_u64 last, led89_u64 count)
{
    unsigned char foot[LED89_SEGMENT_FOOTER_SIZE];
    led89_seg_footer sf;
    int rc;

    rc = led89_digest_range(l, fd, (led89_u64)LED89_SEGMENT_HEADER_SIZE, offset,
                            &sf.segment_digest);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    sf.last_index = last;
    sf.record_count = count;
    sf.body_size = led89_body_size(offset);
    led89_seg_footer_encode(foot, &sf);
    rc = l->io->pwrite(l->io->ctx, fd, foot, sizeof foot, offset);
    return rc;
}

static int led89_rewrite_abort(ledger89 *l, led89_walk *w, led89_fd tmp_fd,
                               int rc)
{
    led89_walk_free(w);
    led89_tmp_close(l, tmp_fd);
    return rc;
}

static int led89_rewrite_step(ledger89 *l, led89_fd tmp_fd, led89_u64 *offset,
                              const led89_rec_header *rh,
                              const unsigned char *payload)
{
    ledger89_record rec;
    led89_u64 bytes;
    int rc;

    rc = led89_u64_to_index(rh->index, &rec.index);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_u64_to_index(rh->tag, &rec.tag);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rec.data = payload;
    rec.size = (size_t)rh->payload_size;
    bytes = led89_batch_bytes(&rec, 1u);
    rc = led89_write_batch(l, tmp_fd, offset, &rec, 1u, rh->index, bytes);
    return rc;
}

static int led89_rewrite_segment(ledger89 *l, led89_fd src_fd,
                                 led89_u64 data_end, const char *target_name,
                                 led89_u64 keep_first, led89_u64 keep_last,
                                 led89_u64 new_first, int sealed,
                                 led89_u64 *records_out, led89_u64 *offset_out)
{
    unsigned char hdr[LED89_SEGMENT_HEADER_SIZE];
    led89_seg_header sh;
    led89_walk w;
    led89_rec_header rh;
    const unsigned char *payload;
    led89_fd tmp_fd;
    led89_u64 offset;
    led89_u64 count;
    int done;
    int rc;

    rc = led89_path_join(l->scratch, l->scratch_cap, l->path, LED89_TMP_NAME);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->open(l->io->ctx, l->scratch,
                     LED89_OPEN_READ | LED89_OPEN_WRITE | LED89_OPEN_CREATE,
                     &tmp_fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->truncate(l->io->ctx, tmp_fd, 0u);
    if (rc != LEDGER89_OK)
    {
        led89_tmp_close(l, tmp_fd);
        return rc;
    }
    sh.first_index = new_first;
    sh.flags = 0u;
    led89_seg_header_encode(hdr, &sh);
    rc = l->io->pwrite(l->io->ctx, tmp_fd, hdr, sizeof hdr, 0u);
    if (rc != LEDGER89_OK)
    {
        led89_tmp_close(l, tmp_fd);
        return rc;
    }
    offset = (led89_u64)LED89_SEGMENT_HEADER_SIZE;
    count = 0u;
    done = 0;
    led89_walk_init(&w, l->io, src_fd, (led89_u64)LED89_SEGMENT_HEADER_SIZE,
                    data_end);
    while (done == 0)
    {
        rc = led89_walk_next(&w, &rh, &payload);
        if (rc == LEDGER89_END)
        {
            done = 1;
        }
        else if (rc != LEDGER89_OK)
        {
            rc = led89_rewrite_abort(l, &w, tmp_fd, rc);
            return rc;
        }
        else if (rh.index > keep_last)
        {
            done = 1;
        }
        else if (rh.index >= keep_first)
        {
            rc = led89_rewrite_step(l, tmp_fd, &offset, &rh, payload);
            if (rc != LEDGER89_OK)
            {
                rc = led89_rewrite_abort(l, &w, tmp_fd, rc);
                return rc;
            }
            ++count;
        }
    }
    led89_walk_free(&w);
    if (sealed != 0)
    {
        rc = led89_write_footer(l, tmp_fd, offset, keep_last, count);
        if (rc != LEDGER89_OK)
        {
            led89_tmp_close(l, tmp_fd);
            return rc;
        }
        offset = led89_add_footer(offset);
    }
    rc = l->io->sync(l->io->ctx, tmp_fd);
    if (rc != LEDGER89_OK)
    {
        led89_tmp_close(l, tmp_fd);
        return rc;
    }
    rc = l->io->close(l->io->ctx, tmp_fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_install_tmp(l, target_name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *records_out = count;
    *offset_out = offset;
    return LEDGER89_OK;
}

/* --- truncation ------------------------------------------------------- */

static int led89_truncate_active(ledger89 *l, led89_u64 target)
{
    led89_u64 count;
    led89_u64 offset;
    int rc;

    rc = led89_rewrite_segment(l, l->active_fd, l->active_offset,
                               LED89_ACTIVE_NAME, l->active_first, target,
                               l->active_first, 0, &count, &offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_active_reopen(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->active_offset = offset;
    l->active_records = count;
    l->last_index = target;
    l->dirty = 0;
    return LEDGER89_OK;
}

static int led89_rewrite_sealed(ledger89 *l, size_t b, led89_u64 keep_first,
                                led89_u64 keep_last, led89_u64 new_first)
{
    led89_fd fd;
    led89_u64 size;
    led89_u64 count;
    led89_u64 offset;
    int rc;

    rc = led89_open_sealed(l, b, &fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->size(l->io->ctx, fd, &size);
    if (rc != LEDGER89_OK)
    {
        l->io->close(l->io->ctx, fd);
        return rc;
    }
    if (size <
        (led89_u64)(LED89_SEGMENT_HEADER_SIZE + LED89_SEGMENT_FOOTER_SIZE))
    {
        l->io->close(l->io->ctx, fd);
        return LEDGER89_ERR_CORRUPT;
    }
    rc = led89_rewrite_segment(
        l, fd, size - (led89_u64)LED89_SEGMENT_FOOTER_SIZE, l->segments[b].name,
        keep_first, keep_last, new_first, 1, &count, &offset);
    l->io->close(l->io->ctx, fd);
    return rc;
}

static int led89_drop_last(ledger89 *l)
{
    int rc;

    rc = led89_unlink_sealed(l, l->segment_count - 1u);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->segment_count -= 1u;
    return LEDGER89_OK;
}

static int led89_truncate_sealed(ledger89 *l, size_t b, led89_u64 target)
{
    int rc;

    rc = led89_unlink_active(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    while (l->segment_count > b + 1u)
    {
        rc = led89_drop_last(l);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    if (target < l->segments[b].last_index)
    {
        rc = led89_rewrite_sealed(l, b, l->segments[b].first_index, target,
                                  l->segments[b].first_index);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
        l->segments[b].last_index = target;
    }
    rc = led89_active_reset(l, target + 1u);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->last_index = target;
    return LEDGER89_OK;
}

static void led89_mark_empty(ledger89 *l)
{
    l->last_index = l->base - 1u;
}

static int led89_truncate_all(ledger89 *l)
{
    size_t i;
    int rc;

    if (l->segment_count == 0u)
    {
        rc = led89_active_reset(l, l->base);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
        led89_mark_empty(l);
        return LEDGER89_OK;
    }
    rc = led89_unlink_active(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    for (i = l->segment_count - 1u; i > 0u; --i)
    {
        rc = led89_unlink_sealed(l, i);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    rc = led89_rename_sealed_active(l, 0u);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->segment_count = 0u;
    rc = led89_active_reset(l, l->base);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->last_index = l->base - 1u;
    return LEDGER89_OK;
}

static int led89_truncate_some(ledger89 *l, led89_u64 target)
{
    size_t b;
    int is_active;
    int rc;

    rc = led89_segment_find(l, target, &b, &is_active);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (is_active != 0)
    {
        rc = led89_truncate_active(l, target);
        return rc;
    }
    rc = led89_truncate_sealed(l, b, target);
    return rc;
}

/* --- discard ---------------------------------------------------------- */

static void led89_segments_drop_first(ledger89 *l)
{
    if (l->segment_count > 1u)
    {
        memmove(&l->segments[0], &l->segments[1],
                (l->segment_count - 1u) * sizeof l->segments[0]);
    }
    l->segment_count -= 1u;
}

static int led89_discard_first(ledger89 *l)
{
    int rc;

    rc = led89_unlink_sealed(l, 0u);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    led89_segments_drop_first(l);
    return LEDGER89_OK;
}

static int led89_discard_active(ledger89 *l, led89_u64 target)
{
    led89_u64 count;
    led89_u64 offset;
    int rc;

    rc = led89_rewrite_segment(l, l->active_fd, l->active_offset,
                               LED89_ACTIVE_NAME, target, l->last_index, target,
                               0, &count, &offset);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_active_reopen(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->active_first = target;
    l->active_offset = offset;
    l->active_records = count;
    l->dirty = 0;
    l->base = target;
    l->first_index = target;
    return LEDGER89_OK;
}

static int led89_discard_some(ledger89 *l, led89_u64 target)
{
    size_t b;
    int is_active;
    int rc;

    rc = led89_segment_find(l, target, &b, &is_active);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (is_active != 0)
    {
        while (l->segment_count > 0u)
        {
            rc = led89_discard_first(l);
            if (rc != LEDGER89_OK)
            {
                return rc;
            }
        }
        rc = led89_discard_active(l, target);
        return rc;
    }
    while (b > 0u)
    {
        rc = led89_discard_first(l);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
        --b;
    }
    if (target > l->segments[0].first_index)
    {
        rc = led89_rewrite_sealed(l, 0u, target, l->segments[0].last_index,
                                  target);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
        l->segments[0].first_index = target;
    }
    l->base = target;
    l->first_index = target;
    return LEDGER89_OK;
}

static int led89_discard_all(ledger89 *l, led89_u64 base_new)
{
    int rc;

    while (l->segment_count > 0u)
    {
        rc = led89_discard_first(l);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    rc = led89_active_reset(l, base_new);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->base = base_new;
    l->first_index = base_new;
    l->last_index = base_new - 1u;
    return LEDGER89_OK;
}

/* --- public API ------------------------------------------------------- */

int ledger89_truncate_after(ledger89 *l, ledger89_index index)
{
    led89_u64 target;
    int rc;

    if (l == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (l->faulted != 0)
    {
        return LEDGER89_ERR_FAULTED;
    }
    target = (led89_u64)index;
    if (target >= l->last_index)
    {
        return LEDGER89_OK;
    }
    if (target < l->base - 1u)
    {
        return LEDGER89_ERR_RANGE;
    }
    if (target == l->base - 1u)
    {
        rc = led89_truncate_all(l);
    }
    else
    {
        rc = led89_truncate_some(l, target);
    }
    if (rc != LEDGER89_OK)
    {
        l->faulted = 1;
    }
    if (rc == LEDGER89_OK)
    {
        ++l->epoch;
    }
    return rc;
}

static led89_u64 led89_last_plus_one(const ledger89 *l)
{
    return l->last_index + 1u;
}

int ledger89_discard_before(ledger89 *l, ledger89_index index)
{
    led89_u64 target;
    int rc;

    if (l == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (l->faulted != 0)
    {
        return LEDGER89_ERR_FAULTED;
    }
    target = (led89_u64)index;
    if (target <= l->first_index)
    {
        return LEDGER89_OK;
    }
    if (target > l->last_index)
    {
        target = led89_last_plus_one(l);
    }
    if (target > l->last_index)
    {
        rc = led89_discard_all(l, target);
    }
    else
    {
        rc = led89_discard_some(l, target);
    }
    if (rc != LEDGER89_OK)
    {
        l->faulted = 1;
    }
    if (rc == LEDGER89_OK)
    {
        ++l->epoch;
    }
    return rc;
}
