/* ledger89.c - public API glue: open/close, state, append, sync, read,
 * iteration, structural operations, and status strings. */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

static int led89_bad_args(const char *path, const led89_io *io)
{
    if (path == NULL)
    {
        return 1;
    }
    if (path[0] == '\0')
    {
        return 1;
    }
    if (io == NULL)
    {
        return 1;
    }
    return 0;
}

static int led89_flag_set(unsigned long flags, unsigned long bit)
{
    if ((flags & bit) != 0UL)
    {
        return 1;
    }
    return 0;
}

static int led89_setup_path(ledger89 *l, const char *path)
{
    size_t n;

    n = strlen(path);
    l->path = (char *)malloc(n + 1u);
    if (l->path == NULL)
    {
        return LEDGER89_ENOMEM;
    }
    memcpy(l->path, path, n + 1u);
    l->scratch_cap = n + (size_t)LED89_NAME_MAX + 2u;
    l->scratch = (char *)malloc(l->scratch_cap);
    if (l->scratch == NULL)
    {
        return LEDGER89_ENOMEM;
    }
    l->scratch2_cap = l->scratch_cap;
    l->scratch2 = (char *)malloc(l->scratch2_cap);
    if (l->scratch2 == NULL)
    {
        return LEDGER89_ENOMEM;
    }
    return LEDGER89_OK;
}

static int led89_acquire_lock(ledger89 *l)
{
    int rc;

    rc = led89_path_join(l->scratch, l->scratch_cap, l->path, LED89_LOCK_NAME);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->lock(l->io->ctx, l->scratch, l->writable, l->writable,
                     &l->lock_fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    l->lock_open = 1;
    return LEDGER89_OK;
}

void led89_free(ledger89 *l)
{
    size_t i;

    if (l == NULL)
    {
        return;
    }
    for (i = 0u; i < l->part_count; ++i)
    {
        led89_part_close(l, &l->parts[i]);
    }
    if (l->lock_open != 0)
    {
        led89_close_quiet(l, l->lock_fd);
    }
    free(l->parts);
    free(l->dir);
    free(l->buf);
    free(l->scratch);
    free(l->scratch2);
    free(l->path);
    free(l);
}

int led89_open_io(ledger89 **out, const char *path, unsigned long flags,
                  const led89_io *io)
{
    ledger89 *l;
    int writable;
    int create;
    int excl;
    int rc;

    if (out == NULL)
    {
        return LEDGER89_EINVAL;
    }
    *out = NULL;
    if (led89_bad_args(path, io) != 0)
    {
        return LEDGER89_EINVAL;
    }
    writable = led89_flag_set(flags, LEDGER89_OPEN_RDWR);
    if (writable == led89_flag_set(flags, LEDGER89_OPEN_RDONLY))
    {
        return LEDGER89_EINVAL;
    }
    create = led89_flag_set(flags, LEDGER89_OPEN_CREATE);
    excl = led89_flag_set(flags, LEDGER89_OPEN_EXCL);
    if (create != 0)
    {
        if (writable == 0)
        {
            return LEDGER89_EINVAL;
        }
    }
    if (excl != 0)
    {
        if (create == 0)
        {
            return LEDGER89_EINVAL;
        }
    }
    l = (ledger89 *)calloc(1u, sizeof *l);
    if (l == NULL)
    {
        return LEDGER89_ENOMEM;
    }
    l->io = io;
    l->writable = writable;
    l->part_target = LED89_PART_TARGET_BYTES;
    rc = led89_setup_path(l, path);
    if (rc == LEDGER89_OK)
    {
        if (writable != 0)
        {
            if (create != 0)
            {
                rc = l->io->mkdir(l->io->ctx, l->path);
            }
        }
    }
    if (rc == LEDGER89_OK)
    {
        rc = led89_acquire_lock(l);
    }
    if (rc == LEDGER89_OK)
    {
        rc = led89_recover(l);
    }
    if (rc == LEDGER89_ENOENT)
    {
        if (create != 0)
        {
            rc = led89_clean_dir_for_create(l);
            if (rc == LEDGER89_OK)
            {
                rc = led89_create_ledger(l);
            }
        }
    }
    else if (rc == LEDGER89_OK)
    {
        if (create != 0)
        {
            if (excl != 0)
            {
                rc = LEDGER89_EEXIST;
            }
        }
    }
    if (rc != LEDGER89_OK)
    {
        led89_free(l);
        return rc;
    }
    *out = l;
    return LEDGER89_OK;
}

int ledger89_open(ledger89 **ledger_out, const char *path, unsigned long flags)
{
    int rc;

    rc = led89_open_io(ledger_out, path, flags, led89_io_posix());
    return rc;
}

void ledger89_close(ledger89 *ledger)
{
    led89_free(ledger);
}

int ledger89_get_state(ledger89 *ledger, ledger89_state *state_out)
{
    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    if (state_out == NULL)
    {
        return LEDGER89_EINVAL;
    }
    state_out->id = ledger->id;
    state_out->revision = led89_to_public(ledger->revision);
    state_out->first = led89_to_public(ledger->first);
    state_out->stable_end = led89_to_public(ledger->stable_end);
    state_out->end = led89_to_public(ledger->end);
    return LEDGER89_OK;
}

static int led89_validate_slices(const ledger89_slice *records, size_t count)
{
    led89_u64 wide_count;
    size_t i;

    if (count == 0u)
    {
        return LEDGER89_EINVAL;
    }
    if (records == NULL)
    {
        return LEDGER89_EINVAL;
    }
    /* The widened local keeps the LP64 range guard free of a spurious
       -Wtype-limits warning on ILP32, where size_t is 32 bits. */
    wide_count = (led89_u64)count;
    if (wide_count > (led89_u64)0xFFFFFFFFu)
    {
        return LEDGER89_ERANGE;
    }
    for (i = 0u; i < count; ++i)
    {
        if (records[i].size > (size_t)LEDGER89_MAX_RECORD_BYTES)
        {
            return LEDGER89_ERANGE;
        }
        if (records[i].size > 0u)
        {
            if (records[i].data == NULL)
            {
                return LEDGER89_EINVAL;
            }
        }
    }
    return LEDGER89_OK;
}

static int led89_append_common(ledger89 *l, const ledger89_slice *records,
                               size_t count, ledger89_index *first_out,
                               int check, led89_u64 expected_revision,
                               led89_u64 expected_end)
{
    led89_u64 total;
    led89_u64 first;
    int rc;

    rc = led89_check_mutable(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_validate_slices(records, count);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (check != 0)
    {
        if (l->revision != expected_revision)
        {
            return LEDGER89_ESTALE;
        }
        if (l->end != expected_end)
        {
            return LEDGER89_ESTALE;
        }
    }
    if (led89_u64_add(l->end, (led89_u64)count, &first) == 0)
    {
        return LEDGER89_EOVERFLOW;
    }
    rc = led89_batch_total(records, count, &total);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (l->end > l->parts[l->active_index].desc.first)
    {
        if (l->parts[l->active_index].bytes + total > l->part_target)
        {
            rc = led89_rotate_impl(l);
            if (rc != LEDGER89_OK)
            {
                return rc;
            }
        }
    }
    rc = led89_active_append(l, records, count, &first);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (first_out != NULL)
    {
        *first_out = led89_to_public(first);
    }
    return LEDGER89_OK;
}

int ledger89_appendv(ledger89 *ledger, const ledger89_slice *records,
                     size_t count, ledger89_index *first_out)
{
    int rc;

    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    rc = led89_append_common(ledger, records, count, first_out, 0, (led89_u64)0,
                             (led89_u64)0);
    return rc;
}

int ledger89_appendv_at(ledger89 *ledger, ledger89_revision expected_revision,
                        ledger89_index expected_end,
                        const ledger89_slice *records, size_t count,
                        ledger89_index *first_out)
{
    int rc;

    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    rc = led89_append_common(ledger, records, count, first_out, 1,
                             led89_from_public(expected_revision),
                             led89_from_public(expected_end));
    return rc;
}

int ledger89_append(ledger89 *ledger, const void *data, size_t size,
                    ledger89_index *index_out)
{
    ledger89_slice slice;
    int rc;

    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    slice.data = data;
    slice.size = size;
    rc = led89_append_common(ledger, &slice, 1u, index_out, 0, (led89_u64)0,
                             (led89_u64)0);
    return rc;
}

int ledger89_sync(ledger89 *ledger, ledger89_index *stable_end_out)
{
    int rc;

    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    rc = led89_check_mutable(ledger);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_sync_impl(ledger);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (stable_end_out != NULL)
    {
        *stable_end_out = led89_to_public(ledger->stable_end);
    }
    return LEDGER89_OK;
}

int ledger89_read(ledger89 *ledger, ledger89_index index, void *data_out,
                  size_t capacity, size_t *size_out)
{
    int rc;

    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    if (size_out == NULL)
    {
        return LEDGER89_EINVAL;
    }
    rc = led89_read_impl(ledger, led89_from_public(index), data_out, capacity,
                         size_out);
    return rc;
}

int ledger89_read_at(ledger89 *ledger, ledger89_index index, size_t offset,
                     void *data_out, size_t size)
{
    int rc;

    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    if (data_out == NULL)
    {
        if (size > 0u)
        {
            return LEDGER89_EINVAL;
        }
    }
    rc = led89_read_at_impl(ledger, led89_from_public(index), offset, data_out,
                            size);
    return rc;
}

int ledger89_iter_init(ledger89_iter *iter, ledger89 *ledger,
                       ledger89_index from)
{
    led89_u64 at;

    if (iter == NULL)
    {
        return LEDGER89_EINVAL;
    }
    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    at = led89_from_public(from);
    iter->ledger = ledger;
    iter->next = from;
    iter->revision = led89_to_public(ledger->revision);
    if (at < ledger->first)
    {
        return LEDGER89_EGONE;
    }
    if (at > ledger->end)
    {
        return LEDGER89_ERANGE;
    }
    return LEDGER89_OK;
}

int ledger89_iter_next(ledger89_iter *iter, ledger89_index *index_out,
                       void *data_out, size_t capacity, size_t *size_out)
{
    ledger89 *l;
    led89_u64 index;
    int rc;

    if (iter == NULL)
    {
        return LEDGER89_EINVAL;
    }
    if (iter->ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    if (size_out == NULL)
    {
        return LEDGER89_EINVAL;
    }
    l = iter->ledger;
    index = led89_from_public(iter->next);
    if (index < l->first)
    {
        return LEDGER89_EGONE;
    }
    if (l->revision != led89_from_public(iter->revision))
    {
        return LEDGER89_ESTALE;
    }
    if (index >= l->end)
    {
        return LEDGER89_DONE;
    }
    rc = led89_read_impl(l, index, data_out, capacity, size_out);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (index_out != NULL)
    {
        *index_out = led89_to_public(index);
    }
    iter->next = led89_to_public(index + (led89_u64)1);
    return LEDGER89_OK;
}

int ledger89_truncate_from(ledger89 *ledger, ledger89_index from)
{
    led89_u64 at;
    int rc;

    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    rc = led89_check_mutable(ledger);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (ledger->end != ledger->stable_end)
    {
        return LEDGER89_EUNSTABLE;
    }
    at = led89_from_public(from);
    if (at < ledger->first)
    {
        return LEDGER89_EGONE;
    }
    if (at > ledger->end)
    {
        return LEDGER89_ERANGE;
    }
    rc = led89_truncate_impl(ledger, at);
    return rc;
}

int ledger89_prune_before(ledger89 *ledger, ledger89_index requested_first,
                          ledger89_index *actual_first_out)
{
    led89_u64 actual;
    int rc;

    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    if (actual_first_out == NULL)
    {
        return LEDGER89_EINVAL;
    }
    rc = led89_check_mutable(ledger);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (led89_from_public(requested_first) > ledger->stable_end)
    {
        return LEDGER89_EUNSTABLE;
    }
    rc = led89_prune_impl(ledger, led89_from_public(requested_first), &actual);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    *actual_first_out = led89_to_public(actual);
    return LEDGER89_OK;
}

int ledger89_rotate(ledger89 *ledger)
{
    int rc;

    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    rc = led89_check_mutable(ledger);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_rotate_impl(ledger);
    return rc;
}

static int led89_verify_step(ledger89 *l, led89_fd fd, led89_u64 *off,
                             led89_u64 *left, led89_u32 *crc)
{
    size_t chunk;
    int rc;

    chunk = led89_chunk_of(*left);
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
    *left = led89_u64_left(*left, (led89_u64)chunk);
    return LEDGER89_OK;
}

static int led89_verify_batch(ledger89 *l, const led89_batch_dir_entry *e)
{
    unsigned char crcbuf[LED89_RECORD_CRC_SIZE];
    led89_u64 off;
    led89_u64 left;
    led89_u32 crc;
    led89_fd fd;
    int rc;

    rc = led89_part_open(l, l->parts[e->part].desc.file_id, LED89_OPEN_READ,
                         &fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    crc = 0u;
    off = e->offset;
    left = e->bytes - (led89_u64)8;
    while (left > (led89_u64)0)
    {
        rc = led89_verify_step(l, fd, &off, &left, &crc);
        if (rc != LEDGER89_OK)
        {
            led89_close_quiet(l, fd);
            return rc;
        }
    }
    rc = l->io->pread(l->io->ctx, fd, crcbuf, sizeof crcbuf, off);
    led89_close_quiet(l, fd);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_EIO;
    }
    if (crc != led89_get_u32(crcbuf))
    {
        return LEDGER89_ECORRUPT;
    }
    return LEDGER89_OK;
}

static int led89_verify_sealed_step(ledger89 *l, led89_fd fd, led89_u64 end,
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

static int led89_verify_sealed(ledger89 *l, size_t part_index)
{
    led89_part *p;
    unsigned char fbuf[LED89_SEALED_FOOTER_SIZE];
    led89_sealed_footer f;
    led89_fd fd;
    led89_u64 end;
    led89_u64 off;
    led89_u32 crc;
    int rc;

    p = &l->parts[part_index];
    rc = led89_part_open(l, p->desc.file_id, LED89_OPEN_READ, &fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    end = led89_u64_left(p->bytes, (led89_u64)LED89_SEALED_FOOTER_SIZE);
    rc = l->io->pread(l->io->ctx, fd, fbuf, sizeof fbuf, end);
    if (rc == LEDGER89_OK)
    {
        rc = led89_sealed_footer_decode(fbuf, &f);
    }
    if (rc != LEDGER89_OK)
    {
        led89_close_quiet(l, fd);
        if (rc == LEDGER89_EIO)
        {
            return rc;
        }
        return LEDGER89_ECORRUPT;
    }
    crc = 0u;
    off = (led89_u64)LED89_PART_HEADER_SIZE;
    while (off < end)
    {
        rc = led89_verify_sealed_step(l, fd, end, &off, &crc);
        if (rc != LEDGER89_OK)
        {
            led89_close_quiet(l, fd);
            return rc;
        }
    }
    led89_close_quiet(l, fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (crc != f.digest)
    {
        return LEDGER89_ECORRUPT;
    }
    return LEDGER89_OK;
}

int ledger89_verify(ledger89 *ledger)
{
    size_t i;
    int rc;

    if (ledger == NULL)
    {
        return LEDGER89_EINVAL;
    }
    for (i = 0u; i + 1u < ledger->part_count; ++i)
    {
        rc = led89_verify_sealed(ledger, i);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    for (i = 0u; i < ledger->dir_count; ++i)
    {
        rc = led89_verify_batch(ledger, &ledger->dir[i]);
        if (rc != LEDGER89_OK)
        {
            return rc;
        }
    }
    return LEDGER89_OK;
}

const char *ledger89_strerror(int result)
{
    switch (result)
    {
    case LEDGER89_OK:
        return "ok";
    case LEDGER89_DONE:
        return "done";
    case LEDGER89_EINVAL:
        return "invalid argument";
    case LEDGER89_ENOMEM:
        return "out of memory";
    case LEDGER89_EIO:
        return "io error";
    case LEDGER89_ENOENT:
        return "not found";
    case LEDGER89_EEXIST:
        return "already exists";
    case LEDGER89_EBUSY:
        return "busy";
    case LEDGER89_EROFS:
        return "read-only";
    case LEDGER89_ECORRUPT:
        return "corrupt";
    case LEDGER89_EFORMAT:
        return "unsupported format";
    case LEDGER89_ESTALE:
        return "stale";
    case LEDGER89_EGONE:
        return "gone";
    case LEDGER89_ERANGE:
        return "out of range";
    case LEDGER89_EUNSTABLE:
        return "unstable";
    case LEDGER89_ETOOSMALL:
        return "buffer too small";
    case LEDGER89_EOVERFLOW:
        return "overflow";
    case LEDGER89_EPOISONED:
        return "poisoned";
    default:
        return "unknown";
    }
}
