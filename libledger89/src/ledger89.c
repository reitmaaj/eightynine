/* ledger89.c - public API glue: open/close, append, sync, observer, index
 * queries, and status strings. */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

static int led89_setup_path(ledger89 *l, const char *path)
{
    size_t n;

    n = strlen(path);
    l->path = (char *)malloc(n + 1u);
    if (l->path == NULL)
    {
        return LEDGER89_ERR_NOMEM;
    }
    memcpy(l->path, path, n + 1u);
    l->scratch_cap = n + (size_t)LED89_NAME_MAX + 2u;
    l->scratch = (char *)malloc(l->scratch_cap);
    if (l->scratch == NULL)
    {
        return LEDGER89_ERR_NOMEM;
    }
    l->scratch2_cap = l->scratch_cap;
    l->scratch2 = (char *)malloc(l->scratch2_cap);
    if (l->scratch2 == NULL)
    {
        return LEDGER89_ERR_NOMEM;
    }
    return LEDGER89_OK;
}

static int led89_lock(ledger89 *l)
{
    int rc;

    rc = led89_path_join(l->scratch, l->scratch_cap, l->path, LED89_LOCK_NAME);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->lock(l->io->ctx, l->scratch, &l->lock_fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->lock_open = 1;
    return LEDGER89_OK;
}

static ledger89_iter *led89_iter_free_next(ledger89_iter *it)
{
    ledger89_iter *next;

    next = it->next;
    led89_iter_free(it);
    return next;
}

static void led89_free_iters(ledger89 *l)
{
    ledger89_iter *it;

    it = l->iters;
    while (it != NULL)
    {
        it = led89_iter_free_next(it);
    }
}

void led89_free(ledger89 *l)
{
    if (l == NULL)
    {
        return;
    }
    led89_free_iters(l);
    if (l->active_open != 0)
    {
        l->io->close(l->io->ctx, l->active_fd);
    }
    if (l->lock_open != 0)
    {
        l->io->close(l->io->ctx, l->lock_fd);
    }
    led89_walk_free(&l->read_walk);
    free(l->segments);
    free(l->scratch);
    free(l->scratch2);
    free(l->path);
    free(l);
}

int led89_open_io(ledger89 **out, const ledger89_config *config,
                  const led89_io *io)
{
    ledger89 *l;
    int rc;

    if (out == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    *out = NULL;
    if (config == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (config->path == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (config->path[0] == '\0')
    {
        return LEDGER89_ERR_ARG;
    }
    if (io == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    l = (ledger89 *)calloc(1u, sizeof *l);
    if (l == NULL)
    {
        return LEDGER89_ERR_NOMEM;
    }
    l->io = io;
    l->active_fd = -1;
    l->lock_fd = -1;
    l->seg_max_bytes = (led89_u64)config->max_segment_bytes;
    if (l->seg_max_bytes == 0u)
    {
        l->seg_max_bytes = (led89_u64)LEDGER89_DEFAULT_SEGMENT_BYTES;
    }
    l->seg_max_records = (led89_u64)config->max_segment_records;
    rc = led89_setup_path(l, config->path);
    if (rc != LEDGER89_OK)
    {
        led89_free(l);
        return rc;
    }
    rc = l->io->mkdir(l->io->ctx, l->path);
    if (rc != LEDGER89_OK)
    {
        led89_free(l);
        return rc;
    }
    rc = led89_lock(l);
    if (rc != LEDGER89_OK)
    {
        led89_free(l);
        return rc;
    }
    rc = led89_recover(l);
    if (rc != LEDGER89_OK)
    {
        led89_free(l);
        return rc;
    }
    *out = l;
    return LEDGER89_OK;
}

int ledger89_open(ledger89 **out, const ledger89_config *config)
{
    int rc;

    rc = led89_open_io(out, config, led89_io_posix());
    return rc;
}

void ledger89_close(ledger89 *l)
{
    led89_free(l);
}

static int led89_validate_record(const ledger89_record *r, led89_u64 expect,
                                 led89_u64 *next)
{
    led89_u64 idx;

    idx = (led89_u64)r->index;
    if (idx != expect)
    {
        return LEDGER89_ERR_SEQUENCE;
    }
    if (r->size > (size_t)LEDGER89_MAX_RECORD_BYTES)
    {
        return LEDGER89_ERR_ARG;
    }
    if (r->size > 0u)
    {
        if (r->data == NULL)
        {
            return LEDGER89_ERR_ARG;
        }
    }
    if (idx == ~(led89_u64)0)
    {
        return LEDGER89_ERR_RANGE;
    }
    *next = idx + 1u;
    return LEDGER89_OK;
}

static int led89_validate_batch(const ledger89 *l,
                                const ledger89_record *records, size_t count)
{
    led89_u64 expect;
    size_t i;
    int rc;

    expect = l->last_index + 1u;
    for (i = 0u; i < count; ++i)
    {
        rc = led89_validate_record(&records[i], expect, &expect);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    return LEDGER89_OK;
}

static void led89_notify_one(ledger89 *l, const ledger89_record *r)
{
    ledger89_view view;

    view.index = r->index;
    view.tag = r->tag;
    view.data = r->data;
    view.size = r->size;
    l->observer(l->observer_ctx, &view);
}

static void led89_notify(ledger89 *l, const ledger89_record *records,
                         size_t count)
{
    size_t i;

    if (l->observer == NULL)
    {
        return;
    }
    for (i = 0u; i < count; ++i)
    {
        led89_notify_one(l, &records[i]);
    }
}

int ledger89_append(ledger89 *l, const ledger89_record *records, size_t count)
{
    led89_u64 bytes;
    int rotate;
    int rc;

    if (l == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (l->faulted != 0)
    {
        return LEDGER89_ERR_FAULTED;
    }
    if (count == 0u)
    {
        return LEDGER89_ERR_ARG;
    }
    if (records == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if ((led89_u64)count > (led89_u64)0xFFFFFFFFu)
    {
        return LEDGER89_ERR_ARG;
    }
    rc = led89_validate_batch(l, records, count);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    bytes = led89_batch_bytes(records, count);
    rotate = led89_should_rotate(l, (led89_u64)count, bytes);
    if (rotate != 0)
    {
        rc = ledger89_rotate(l);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    rc = led89_active_append(l, records, count);
    if (rc != LEDGER89_OK)
    {
        l->faulted = 1;
        return LEDGER89_ERR_IO;
    }
    led89_notify(l, records, count);
    return LEDGER89_OK;
}

int ledger89_sync(ledger89 *l)
{
    int rc;

    if (l == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (l->faulted != 0)
    {
        return LEDGER89_ERR_FAULTED;
    }
    if (l->dirty == 0)
    {
        return LEDGER89_OK;
    }
    rc = l->io->sync(l->io->ctx, l->active_fd);
    if (rc != LEDGER89_OK)
    {
        l->faulted = 1;
        return LEDGER89_ERR_IO;
    }
    l->dirty = 0;
    return LEDGER89_OK;
}

int ledger89_rotate(ledger89 *l)
{
    int rc;

    if (l == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (l->faulted != 0)
    {
        return LEDGER89_ERR_FAULTED;
    }
    if (l->active_records == 0u)
    {
        return LEDGER89_OK;
    }
    rc = led89_seal_active(l);
    if (rc != LEDGER89_OK)
    {
        l->faulted = 1;
        return rc;
    }
    ++l->epoch;
    return LEDGER89_OK;
}

int ledger89_set_observer(ledger89 *l, ledger89_observer_fn fn, void *ctx)
{
    if (l == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    l->observer = fn;
    l->observer_ctx = ctx;
    return LEDGER89_OK;
}

ledger89_index ledger89_first_index(const ledger89 *l)
{
    if (l == NULL)
    {
        return 0ul;
    }
    if (l->first_index > (led89_u64)ULONG_MAX)
    {
        return ULONG_MAX;
    }
    return (ledger89_index)l->first_index;
}

ledger89_index ledger89_last_index(const ledger89 *l)
{
    if (l == NULL)
    {
        return 0ul;
    }
    if (l->last_index > (led89_u64)ULONG_MAX)
    {
        return ULONG_MAX;
    }
    return (ledger89_index)l->last_index;
}

int ledger89_read(ledger89 *l, ledger89_index index, ledger89_view *out)
{
    int rc;

    if (l == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (out == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (index == 0ul)
    {
        return LEDGER89_ERR_ARG;
    }
    rc = led89_read_impl(l, (led89_u64)index, out);
    return rc;
}

int ledger89_iter_open(ledger89 *l, ledger89_index first, ledger89_index last,
                       ledger89_iter **out)
{
    int rc;

    if (l == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    if (out == NULL)
    {
        return LEDGER89_ERR_ARG;
    }
    *out = NULL;
    rc = led89_iter_open_impl(l, (led89_u64)first, (led89_u64)last, out);
    return rc;
}

const char *ledger89_strerror(int status)
{
    switch (status)
    {
    case LEDGER89_OK:
        return "ok";
    case LEDGER89_END:
        return "end";
    case LEDGER89_ERR_ARG:
        return "invalid argument";
    case LEDGER89_ERR_IO:
        return "io error";
    case LEDGER89_ERR_NOMEM:
        return "out of memory";
    case LEDGER89_ERR_NOTFOUND:
        return "not found";
    case LEDGER89_ERR_RANGE:
        return "out of range";
    case LEDGER89_ERR_SEQUENCE:
        return "sequence error";
    case LEDGER89_ERR_CORRUPT:
        return "corrupt";
    case LEDGER89_ERR_BUSY:
        return "busy";
    case LEDGER89_ERR_FAULTED:
        return "faulted";
    case LEDGER89_ERR_STATE:
        return "invalid state";
    default:
        return "unknown";
    }
}
