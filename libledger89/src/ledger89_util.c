/* ledger89_util.c - pure path/name helpers, buffer growth, and shared
 * handle-state guards. */

#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

static size_t led89_double_size(size_t v)
{
    return v * 2u;
}

static size_t led89_max_size(size_t a, size_t b)
{
    if (a > b)
    {
        return a;
    }
    return b;
}

static int led89_hex_value(char c)
{
    if (c >= '0')
    {
        if (c <= '9')
        {
            return c - '0';
        }
    }
    if (c >= 'a')
    {
        if (c <= 'f')
        {
            return (c - 'a') + 10;
        }
    }
    return -1;
}

int led89_path_join(char *out, size_t cap, const char *dir, const char *name)
{
    size_t dlen;
    size_t nlen;

    dlen = strlen(dir);
    nlen = strlen(name);
    if (dlen + 1u + nlen + 1u > cap)
    {
        return LEDGER89_ERANGE;
    }
    memcpy(out, dir, dlen);
    out[dlen] = '/';
    memcpy(out + dlen + 1u, name, nlen + 1u);
    return LEDGER89_OK;
}

static char led89_hex_digit(led89_u64 value, unsigned int i)
{
    static const char digits[] = "0123456789abcdef";
    unsigned int shift;

    shift = (15u - i) * 4u;
    return digits[(unsigned int)((value >> shift) & 0x0Fu)];
}

static void led89_hex16(char *out, led89_u64 value)
{
    unsigned int i;

    for (i = 0u; i < 16u; ++i)
    {
        out[i] = led89_hex_digit(value, i);
    }
    out[16] = '\0';
}

void led89_part_name(char *out, led89_u64 file_id)
{
    memcpy(out, LED89_PART_PREFIX, sizeof(LED89_PART_PREFIX) - 1u);
    led89_hex16(out + sizeof(LED89_PART_PREFIX) - 1u, file_id);
}

void led89_manifest_name(char *out, led89_u64 generation)
{
    memcpy(out, LED89_MANIFEST_PREFIX, sizeof(LED89_MANIFEST_PREFIX) - 1u);
    led89_hex16(out + sizeof(LED89_MANIFEST_PREFIX) - 1u, generation);
}

static led89_u64 led89_hex_accum(led89_u64 value, led89_u64 digit)
{
    return (value << 4) | digit;
}

static int led89_hex16_parse(const char *in, led89_u64 *out)
{
    led89_u64 value;
    unsigned int i;

    value = 0u;
    for (i = 0u; i < 16u; ++i)
    {
        int digit;

        digit = led89_hex_value(in[i]);
        if (digit < 0)
        {
            return 0;
        }
        value = led89_hex_accum(value, (led89_u64)digit);
    }
    *out = value;
    return 1;
}

int led89_part_name_parse(const char *name, led89_u64 *file_id)
{
    size_t plen;
    int rc;

    plen = sizeof(LED89_PART_PREFIX) - 1u;
    if (strlen(name) != plen + 16u)
    {
        return 0;
    }
    rc = memcmp(name, LED89_PART_PREFIX, plen);
    if (rc != 0)
    {
        return 0;
    }
    rc = led89_hex16_parse(name + plen, file_id);
    return rc;
}

int led89_manifest_name_parse(const char *name, led89_u64 *generation)
{
    size_t plen;
    int rc;

    plen = sizeof(LED89_MANIFEST_PREFIX) - 1u;
    if (strlen(name) != plen + 16u)
    {
        return 0;
    }
    rc = memcmp(name, LED89_MANIFEST_PREFIX, plen);
    if (rc != 0)
    {
        return 0;
    }
    rc = led89_hex16_parse(name + plen, generation);
    return rc;
}

static size_t led89_next_cap(size_t cap, size_t need, size_t initial)
{
    size_t next;

    next = led89_double_size(cap);
    if (cap == 0u)
    {
        next = initial;
    }
    next = led89_max_size(next, need);
    return next;
}

int led89_handle_reserve_parts(ledger89 *l, size_t need)
{
    led89_part *grown;
    size_t next;

    if (need <= l->part_cap)
    {
        return LEDGER89_OK;
    }
    next = led89_next_cap(l->part_cap, need, 8u);
    grown = (led89_part *)realloc(l->parts, next * sizeof(led89_part));
    if (grown == NULL)
    {
        return LEDGER89_ENOMEM;
    }
    l->parts = grown;
    l->part_cap = next;
    return LEDGER89_OK;
}

int led89_handle_reserve_dir(ledger89 *l, size_t need)
{
    led89_batch_dir_entry *grown;
    size_t next;

    if (need <= l->dir_cap)
    {
        return LEDGER89_OK;
    }
    next = led89_next_cap(l->dir_cap, need, 64u);
    grown = (led89_batch_dir_entry *)realloc(
        l->dir, next * sizeof(led89_batch_dir_entry));
    if (grown == NULL)
    {
        return LEDGER89_ENOMEM;
    }
    l->dir = grown;
    l->dir_cap = next;
    return LEDGER89_OK;
}

int led89_buf_reserve(ledger89 *l, size_t need)
{
    unsigned char *grown;
    size_t next;

    if (need <= l->buf_cap)
    {
        return LEDGER89_OK;
    }
    next = led89_next_cap(l->buf_cap, need, 4096u);
    grown = (unsigned char *)realloc(l->buf, next);
    if (grown == NULL)
    {
        return LEDGER89_ENOMEM;
    }
    l->buf = grown;
    l->buf_cap = next;
    return LEDGER89_OK;
}

void led89_close_quiet(ledger89 *l, led89_fd fd)
{
    (void)l->io->close(l->io->ctx, fd);
}

void led89_unlink_quiet(ledger89 *l, const char *path)
{
    (void)l->io->unlink(l->io->ctx, path);
}

void led89_list_close_quiet(ledger89 *l, led89_dir *dir)
{
    (void)l->io->list_close(l->io->ctx, dir);
}

void led89_sync_dir_quiet(ledger89 *l)
{
    (void)l->io->sync_dir(l->io->ctx, l->path);
}

led89_u64 led89_u64_left(led89_u64 total, led89_u64 done)
{
    return total - done;
}

size_t led89_chunk_of(led89_u64 left)
{
    if (left > (led89_u64)4096)
    {
        return 4096u;
    }
    return (size_t)left;
}

led89_u64 led89_at_add(led89_u64 at, led89_u64 n)
{
    return at + n;
}

int led89_io_error(ledger89 *l, int rc)
{
    if (l != NULL)
    {
        l->poisoned = 1;
    }
    return rc;
}

int led89_check_writable(ledger89 *l)
{
    if (l->writable == 0)
    {
        return LEDGER89_EROFS;
    }
    return LEDGER89_OK;
}

int led89_check_mutable(ledger89 *l)
{
    int rc;

    rc = led89_check_writable(l);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    if (l->poisoned != 0)
    {
        return LEDGER89_EPOISONED;
    }
    return LEDGER89_OK;
}
