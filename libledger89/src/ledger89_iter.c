/* ledger89_iter.c - random read and ordered iteration.
 *
 * Both paths validate record CRCs and batch framing while traversing, so a
 * corrupted record is reported rather than silently skipped. Views point
 * into the walker's reusable buffer and remain valid until the next call on
 * the same handle (read) or iterator (iteration). */

#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

static void led89_iter_unlink(ledger89_iter *it);

static led89_u64 led89_lo_bound(led89_u64 first, const ledger89 *l)
{
    if (first == 0u)
    {
        return l->first_index;
    }
    if (first < l->first_index)
    {
        return l->first_index;
    }
    return first;
}

static led89_u64 led89_hi_bound(led89_u64 last, const ledger89 *l)
{
    if (last == 0u)
    {
        return l->last_index;
    }
    if (last > l->last_index)
    {
        return l->last_index;
    }
    return last;
}

static int led89_fill_view(const led89_rec_header *rh,
                           const unsigned char *payload, ledger89_view *out)
{
    ledger89_index value;
    int rc;

    rc = led89_u64_to_index(rh->index, &value);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    out->index = value;
    rc = led89_u64_to_index(rh->tag, &value);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    out->tag = value;
    out->data = payload;
    out->size = (size_t)rh->payload_size;
    return LEDGER89_OK;
}

static int led89_walk_find(led89_walk *w, led89_u64 index, led89_rec_header *rh,
                           const unsigned char **payload)
{
    int rc;

    for (;;)
    {
        rc = led89_walk_next(w, rh, payload);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
        if (rh->index == index)
        {
            return LEDGER89_OK;
        }
        if (rh->index > index)
        {
            return LEDGER89_ERR_CORRUPT;
        }
    }
}

static int led89_segment_hit(size_t *pos, int *is_active, size_t i)
{
    *pos = i;
    *is_active = 0;
    return LEDGER89_OK;
}

int led89_segment_find(const ledger89 *l, led89_u64 index, size_t *pos,
                       int *is_active)
{
    size_t i;
    int rc;

    if (l->segment_count == 0u)
    {
        *is_active = 1;
        return LEDGER89_OK;
    }
    if (index > l->segments[l->segment_count - 1u].last_index)
    {
        *is_active = 1;
        return LEDGER89_OK;
    }
    for (i = 0u; i < l->segment_count; ++i)
    {
        if (index <= l->segments[i].last_index)
        {
            rc = led89_segment_hit(pos, is_active, i);
            return rc;
        }
    }
    return LEDGER89_ERR_CORRUPT;
}

static int led89_read_open_active(const ledger89 *l, led89_fd *fd, int *own,
                                  led89_u64 *end)
{
    *fd = l->active_fd;
    *own = 0;
    *end = l->active_offset;
    return LEDGER89_OK;
}

static int led89_read_open(const ledger89 *l, size_t pos, int is_active,
                           led89_fd *fd, int *own, led89_u64 *start,
                           led89_u64 *end)
{
    led89_u64 size;
    int rc;

    *start = (led89_u64)LED89_SEGMENT_HEADER_SIZE;
    if (is_active != 0)
    {
        rc = led89_read_open_active(l, fd, own, end);
        return rc;
    }
    rc = led89_path_join(l->scratch, l->scratch_cap, l->path,
                         l->segments[pos].name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->open(l->io->ctx, l->scratch, LED89_OPEN_READ, fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *own = 1;
    rc = l->io->size(l->io->ctx, *fd, &size);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (size <
        (led89_u64)(LED89_SEGMENT_HEADER_SIZE + LED89_SEGMENT_FOOTER_SIZE))
    {
        return LEDGER89_ERR_CORRUPT;
    }
    *end = size - (led89_u64)LED89_SEGMENT_FOOTER_SIZE;
    return LEDGER89_OK;
}

int led89_read_impl(ledger89 *l, led89_u64 index, ledger89_view *out)
{
    size_t pos;
    int is_active;
    led89_fd fd;
    int own;
    led89_u64 start;
    led89_u64 end;
    led89_walk *w;
    led89_rec_header rh;
    const unsigned char *payload;
    int rc;

    if (index < l->first_index)
    {
        return LEDGER89_ERR_NOTFOUND;
    }
    if (index > l->last_index)
    {
        return LEDGER89_ERR_NOTFOUND;
    }
    rc = led89_segment_find(l, index, &pos, &is_active);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_read_open(l, pos, is_active, &fd, &own, &start, &end);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    w = &l->read_walk;
    led89_walk_restart(w, l->io, fd, start, end);
    rc = led89_walk_find(w, index, &rh, &payload);
    if (rc == LEDGER89_OK)
    {
        rc = led89_fill_view(&rh, payload, out);
    }
    if (own != 0)
    {
        l->io->close(l->io->ctx, fd);
    }
    return rc;
}

static void led89_iter_close_walk(ledger89_iter *it)
{
    if (it->own_fd != 0)
    {
        it->l->io->close(it->l->io->ctx, it->fd);
    }
    led89_walk_free(&it->walk);
    it->walk_open = 0;
    it->own_fd = 0;
    it->fd = -1;
}

static int led89_iter_open_active_walk(ledger89_iter *it)
{
    led89_walk_init(&it->walk, it->l->io, it->l->active_fd,
                    (led89_u64)LED89_SEGMENT_HEADER_SIZE, it->l->active_offset);
    it->walk_open = 1;
    return LEDGER89_OK;
}

static int led89_iter_open_walk(ledger89_iter *it)
{
    ledger89 *l;
    led89_u64 size;
    int rc;

    l = it->l;
    if (it->in_active != 0)
    {
        rc = led89_iter_open_active_walk(it);
        return rc;
    }
    rc = led89_path_join(l->scratch, l->scratch_cap, l->path,
                         l->segments[it->seg_pos].name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->open(l->io->ctx, l->scratch, LED89_OPEN_READ, &it->fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    it->own_fd = 1;
    rc = l->io->size(l->io->ctx, it->fd, &size);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (size <
        (led89_u64)(LED89_SEGMENT_HEADER_SIZE + LED89_SEGMENT_FOOTER_SIZE))
    {
        return LEDGER89_ERR_CORRUPT;
    }
    led89_walk_init(&it->walk, l->io, it->fd,
                    (led89_u64)LED89_SEGMENT_HEADER_SIZE,
                    size - (led89_u64)LED89_SEGMENT_FOOTER_SIZE);
    it->walk_open = 1;
    return LEDGER89_OK;
}

static int led89_iter_locate(ledger89_iter *it)
{
    size_t pos;
    int is_active;
    int rc;

    rc = led89_segment_find(it->l, it->next_index, &pos, &is_active);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    it->seg_pos = pos;
    it->in_active = is_active;
    return LEDGER89_OK;
}

static int led89_iter_step(ledger89_iter *it, ledger89_view *out)
{
    led89_rec_header rh;
    const unsigned char *payload;
    int rc;

    for (;;)
    {
        rc = led89_walk_next(&it->walk, &rh, &payload);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
        if (rh.index > it->next_index)
        {
            return LEDGER89_ERR_CORRUPT;
        }
        if (rh.index == it->next_index)
        {
            break;
        }
    }
    rc = led89_fill_view(&rh, payload, out);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    it->next_index += 1u;
    return LEDGER89_OK;
}

static int led89_iter_advance(ledger89_iter *it)
{
    int rc;

    led89_iter_close_walk(it);
    if (it->in_active != 0)
    {
        if (it->next_index <= it->end_index)
        {
            return LEDGER89_ERR_CORRUPT;
        }
        return LEDGER89_END;
    }
    it->seg_pos += 1u;
    if (it->seg_pos >= it->l->segment_count)
    {
        it->in_active = 1;
    }
    rc = led89_iter_open_walk(it);
    return rc;
}

static int led89_iter_pump_once(ledger89_iter *it, ledger89_view *out)
{
    int rc;

    if (it->next_index > it->end_index)
    {
        return LEDGER89_END;
    }
    if (it->walk_open == 0)
    {
        rc = led89_iter_open_walk(it);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    rc = led89_iter_step(it, out);
    return rc;
}

static int led89_iter_pump(ledger89_iter *it, ledger89_view *out)
{
    int rc;

    for (;;)
    {
        rc = led89_iter_pump_once(it, out);
        if (rc == LEDGER89_OK)
        {
            return LEDGER89_OK;
        }
        if (rc != LEDGER89_END)
        {
            return rc;
        }
        if (it->next_index > it->end_index)
        {
            return LEDGER89_END;
        }
        rc = led89_iter_advance(it);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
}

int led89_iter_next_impl(ledger89_iter *it, ledger89_view *out)
{
    int rc;

    if (it == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (out == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (it->l == NULL)
    {
        return LEDGER89_ERR_STATE;
    }
    if (it->l->epoch != it->epoch)
    {
        return LEDGER89_ERR_STATE;
    }
    rc = led89_iter_pump(it, out);
    return rc;
}

static ledger89_iter **led89_next_slot(ledger89_iter **pp)
{
    return &(*pp)->next;
}

static void led89_iter_relink(ledger89_iter **pp)
{
    *pp = (*pp)->next;
}

static void led89_iter_unlink(ledger89_iter *it)
{
    ledger89_iter **pp;

    pp = &it->l->iters;
    while (*pp != NULL)
    {
        if (*pp == it)
        {
            led89_iter_relink(pp);
            return;
        }
        pp = led89_next_slot(pp);
    }
}

static void led89_iter_make_empty(ledger89_iter *it, ledger89_iter **out)
{
    it->next_index = 1u;
    it->end_index = 0u;
    *out = it;
}

static void led89_iter_discard(ledger89_iter *it)
{
    led89_iter_unlink(it);
    led89_iter_free(it);
}

int led89_iter_open_impl(ledger89 *l, led89_u64 first, led89_u64 last,
                         ledger89_iter **out)
{
    ledger89_iter *it;
    led89_u64 lo;
    led89_u64 hi;
    int rc;

    if (first != 0u)
    {
        if (last != 0u)
        {
            if (first > last)
            {
                return LEDGER89_ERR_ARG;
            }
        }
    }
    lo = led89_lo_bound(first, l);
    hi = led89_hi_bound(last, l);
    it = (ledger89_iter *)calloc(1u, sizeof *it);
    if (it == NULL)
    {
        return LEDGER89_ERR_NOMEM;
    }
    it->l = l;
    it->epoch = l->epoch;
    it->fd = -1;
    it->next = l->iters;
    l->iters = it;
    if (lo > hi)
    {
        led89_iter_make_empty(it, out);
        return LEDGER89_OK;
    }
    it->next_index = lo;
    it->end_index = hi;
    rc = led89_iter_locate(it);
    if (rc != LEDGER89_OK)
    {
        led89_iter_discard(it);
        return rc;
    }
    *out = it;
    return LEDGER89_OK;
}

void led89_iter_free(ledger89_iter *it)
{
    if (it == NULL)
    {
        return;
    }
    if (it->walk_open != 0)
    {
        led89_iter_close_walk(it);
    }
    free(it);
}

int ledger89_iter_next(ledger89_iter *it, ledger89_view *out)
{
    int rc;

    rc = led89_iter_next_impl(it, out);
    return rc;
}

void ledger89_iter_close(ledger89_iter *it)
{
    if (it == NULL)
    {
        return;
    }
    led89_iter_unlink(it);
    led89_iter_free(it);
}
