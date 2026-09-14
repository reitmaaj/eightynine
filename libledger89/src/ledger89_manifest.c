/* ledger89_manifest.c - CURRENT pointer and immutable manifest generations.
 *
 * The manifest chooses the committed physical topology; stable markers choose
 * the committed data frontier inside the active part. A manifest is written
 * and fsynced before CURRENT is atomically replaced. */
#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

static unsigned char *led89_alloc_bytes(size_t n)
{
    unsigned char *p;

    p = (unsigned char *)malloc(n);
    return p;
}

static led89_u64 led89_desc_id(const led89_part_desc *d)
{
    return d->file_id;
}

static void led89_release(ledger89 *l, unsigned char *buf, led89_fd fd)
{
    free(buf);
    led89_close_quiet(l, fd);
}

static void led89_unlink_name(ledger89 *l, const char *name)
{
    int rc;

    rc = led89_path_join(l->scratch, l->scratch_cap, l->path, name);
    if (rc == LEDGER89_OK)
    {
        led89_unlink_quiet(l, l->scratch);
    }
}

static int led89_read_file(ledger89 *l, const char *name, unsigned char **out,
                           size_t *size_out)
{
    char *path;
    led89_fd fd;
    led89_u64 size;
    unsigned char *buf;
    int rc;

    *out = NULL;
    *size_out = 0u;
    path = l->scratch;
    rc = led89_path_join(path, l->scratch_cap, l->path, name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->open(l->io->ctx, path, LED89_OPEN_READ, &fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->size(l->io->ctx, fd, &size);
    if (rc != LEDGER89_OK)
    {
        led89_close_quiet(l, fd);
        return rc;
    }
    if (size > (led89_u64)0)
    {
        if (led89_u64_to_size(size, size_out) != LEDGER89_OK)
        {
            led89_close_quiet(l, fd);
            return LEDGER89_ERANGE;
        }
        buf = led89_alloc_bytes(*size_out);
        if (buf == NULL)
        {
            led89_close_quiet(l, fd);
            return LEDGER89_ENOMEM;
        }
        rc = l->io->pread(l->io->ctx, fd, buf, *size_out, (led89_u64)0);
        if (rc != LEDGER89_OK)
        {
            led89_release(l, buf, fd);
            return rc;
        }
        *out = buf;
    }
    led89_close_quiet(l, fd);
    return LEDGER89_OK;
}

static int led89_write_new_file(ledger89 *l, const char *name, const void *data,
                                size_t size)
{
    char *path;
    led89_fd fd;
    int close_rc;
    int rc;

    path = l->scratch;
    rc = led89_path_join(path, l->scratch_cap, l->path, name);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->open(l->io->ctx, path, LED89_OPEN_WRITE | LED89_OPEN_CREATE,
                     &fd);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->truncate(l->io->ctx, fd, (led89_u64)0);
    if (rc == LEDGER89_OK)
    {
        rc = l->io->pwrite(l->io->ctx, fd, data, size, (led89_u64)0);
    }
    if (rc == LEDGER89_OK)
    {
        rc = l->io->sync(l->io->ctx, fd);
    }
    close_rc = l->io->close(l->io->ctx, fd);
    if (close_rc != LEDGER89_OK)
    {
        if (rc == LEDGER89_OK)
        {
            rc = LEDGER89_EIO;
        }
    }
    return rc;
}

int led89_current_read(ledger89 *l, led89_current *c)
{
    unsigned char *buf;
    size_t size;
    int rc;

    rc = led89_read_file(l, LED89_CURRENT_NAME, &buf, &size);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (size != (size_t)LED89_CURRENT_SIZE)
    {
        free(buf);
        return LEDGER89_ECORRUPT;
    }
    rc = led89_current_decode(buf, c);
    free(buf);
    return rc;
}

int led89_current_publish(ledger89 *l, led89_u64 generation)
{
    led89_current c;
    unsigned char buf[LED89_CURRENT_SIZE];
    int rc;

    c.generation = generation;
    led89_current_encode(buf, &c);
    rc = led89_write_new_file(l, LED89_CURRENT_TMP_NAME, buf,
                              (size_t)LED89_CURRENT_SIZE);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_path_join(l->scratch, l->scratch_cap, l->path,
                         LED89_CURRENT_NAME);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ERANGE;
    }
    rc = led89_path_join(l->scratch2, l->scratch2_cap, l->path,
                         LED89_CURRENT_TMP_NAME);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_ERANGE;
    }
    rc = l->io->rename(l->io->ctx, l->scratch2, l->scratch);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->sync_dir(l->io->ctx, l->path);
    return rc;
}

int led89_manifest_read(ledger89 *l, led89_u64 generation, led89_manifest *m)
{
    char name[LED89_NAME_MAX];
    unsigned char *buf;
    size_t size;
    int rc;

    led89_manifest_name(name, generation);
    rc = led89_read_file(l, name, &buf, &size);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = led89_manifest_decode(buf, size, m);
    free(buf);
    return rc;
}

int led89_manifest_publish(ledger89 *l, const led89_manifest *m)
{
    char name[LED89_NAME_MAX];
    unsigned char *buf;
    size_t size;
    int rc;

    size = led89_manifest_bytes(m->sealed_count);
    buf = (unsigned char *)malloc(size);
    if (buf == NULL)
    {
        return LEDGER89_ENOMEM;
    }
    led89_manifest_encode(buf, m);
    led89_manifest_name(name, m->generation);
    rc = led89_write_new_file(l, name, buf, size);
    free(buf);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    rc = l->io->sync_dir(l->io->ctx, l->path);
    return rc;
}

static int led89_manifest_has_part(const led89_manifest *m, led89_u64 file_id)
{
    led89_u32 i;

    if (led89_u64_cmp(m->active.file_id, file_id) == 0)
    {
        return 1;
    }
    for (i = 0u; i < m->sealed_count; ++i)
    {
        if (led89_u64_cmp(m->sealed[i].file_id, file_id) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int led89_gc_orphans(ledger89 *l, const led89_manifest *keep)
{
    led89_dir *dir;
    char name[LED89_NAME_MAX];
    led89_u64 max_id;
    int done;
    int rc;

    max_id = (led89_u64)0;
    if (led89_u64_cmp(keep->active.file_id, max_id) > 0)
    {
        max_id = led89_desc_id(&keep->active);
    }
    rc = l->io->list_open(l->io->ctx, l->path, &dir);
    if (rc != LEDGER89_OK)
    {
        return LEDGER89_OK;
    }
    done = 0;
    while (done == 0)
    {
        led89_u64 file_id;

        rc = l->io->list_next(l->io->ctx, dir, name, sizeof name, &done);
        if (rc != LEDGER89_OK)
        {
            led89_list_close_quiet(l, dir);
            return rc;
        }
        if (done != 0)
        {
            break;
        }
        if (strcmp(name, LED89_CURRENT_TMP_NAME) == 0)
        {
            led89_unlink_name(l, name);
            continue;
        }
        if (led89_part_name_parse(name, &file_id) == 0)
        {
            continue;
        }
        if (led89_u64_cmp(file_id, max_id) > 0)
        {
            max_id = file_id;
        }
        if (led89_manifest_has_part(keep, file_id) == 0)
        {
            led89_unlink_name(l, name);
        }
    }
    led89_list_close_quiet(l, dir);
    l->next_file_id = led89_u64_inc(max_id);
    return LEDGER89_OK;
}
