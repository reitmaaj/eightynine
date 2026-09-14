/* ledger89_format.c - little-endian field codecs and on-disk structure
 * encode/decode for format version 2. No native structure is ever written. */

#include <stdlib.h>
#include <string.h>

#include "ledger89_internal.h"

#define LED89_CURRENT_MAGIC "LD89CUR2"
#define LED89_MANIFEST_MAGIC "LD89MAN2"
#define LED89_PART_MAGIC "LD89PRT2"
#define LED89_BATCH_HEADER_MAGIC "B89\x02"
#define LED89_BATCH_FOOTER_MAGIC "b89\x02"
#define LED89_MARKER_MAGIC "LD89STB2"
#define LED89_MARKER_TRAILER "89STABLE"
#define LED89_SEALED_FOOTER_MAGIC "LD89SGF2"

void led89_put_u16(unsigned char *out, led89_u16 v)
{
    out[0] = (unsigned char)(v & 0xFFu);
    out[1] = (unsigned char)((v >> 8) & 0xFFu);
}

void led89_put_u32(unsigned char *out, led89_u32 v)
{
    out[0] = (unsigned char)(v & 0xFFu);
    out[1] = (unsigned char)((v >> 8) & 0xFFu);
    out[2] = (unsigned char)((v >> 16) & 0xFFu);
    out[3] = (unsigned char)((v >> 24) & 0xFFu);
}

void led89_put_u64(unsigned char *out, led89_u64 v)
{
    led89_put_u32(out, (led89_u32)(v & (led89_u64)0xFFFFFFFFu));
    led89_put_u32(out + 4, (led89_u32)(v >> 32));
}

led89_u16 led89_get_u16(const unsigned char *in)
{
    return (led89_u16)((led89_u16)in[0] | ((led89_u16)in[1] << 8));
}

led89_u32 led89_get_u32(const unsigned char *in)
{
    return (led89_u32)in[0] | ((led89_u32)in[1] << 8) |
           ((led89_u32)in[2] << 16) | ((led89_u32)in[3] << 24);
}

led89_u64 led89_get_u64(const unsigned char *in)
{
    return (led89_u64)led89_get_u32(in) |
           ((led89_u64)led89_get_u32(in + 4) << 32);
}

int led89_bytes_equal(const unsigned char *a, const unsigned char *b, size_t n)
{
    size_t i;

    for (i = 0; i < n; ++i)
    {
        if (a[i] != b[i])
        {
            return 0;
        }
    }
    return 1;
}

/* CURRENT --------------------------------------------------------------- */

void led89_current_encode(unsigned char *out, const led89_current *c)
{
    memset(out, 0, LED89_CURRENT_SIZE);
    memcpy(out, LED89_CURRENT_MAGIC, 8u);
    led89_put_u64(out + 8, c->generation);
    led89_put_u32(out + 16, LED89_FORMAT_VERSION);
    led89_put_u32(out + 20, led89_crc32c(0u, out, 20u));
}

int led89_current_decode(const unsigned char *in, led89_current *c)
{
    if (memcmp(in, LED89_CURRENT_MAGIC, 8u) != 0)
    {
        return LEDGER89_ECORRUPT;
    }
    if (led89_get_u32(in + 16) != LED89_FORMAT_VERSION)
    {
        return LEDGER89_EFORMAT;
    }
    if (led89_get_u32(in + 20) != led89_crc32c(0u, in, 20u))
    {
        return LEDGER89_ECORRUPT;
    }
    c->generation = led89_get_u64(in + 8);
    return LEDGER89_OK;
}

/* MANIFEST -------------------------------------------------------------- */

size_t led89_manifest_bytes(led89_u32 sealed_count)
{
    return (size_t)LED89_MANIFEST_HEADER_SIZE +
           ((size_t)sealed_count * (size_t)LED89_SEALED_DESC_SIZE) +
           (size_t)LED89_ACTIVE_DESC_SIZE + (size_t)LED89_MANIFEST_CRC_SIZE;
}

static void led89_manifest_encode_header(unsigned char *out,
                                         const led89_manifest *m)
{
    memset(out, 0, LED89_MANIFEST_HEADER_SIZE);
    memcpy(out, LED89_MANIFEST_MAGIC, 8u);
    led89_put_u64(out + 8, m->generation);
    memcpy(out + 16, m->uuid, 16u);
    led89_put_u64(out + 32, m->revision);
    led89_put_u64(out + 40, m->first);
    led89_put_u32(out + 48, m->sealed_count);
    led89_put_u32(out + 52, LED89_FORMAT_VERSION);
    led89_put_u32(out + 56, led89_crc32c(0u, out, 56u));
}

static size_t led89_off_inc(size_t off, size_t by)
{
    return off + by;
}

static led89_part_desc *led89_alloc_descs(size_t count)
{
    led89_part_desc *p;

    p = (led89_part_desc *)malloc(count * sizeof(led89_part_desc));
    return p;
}

static size_t led89_encode_desc(unsigned char *out, size_t off,
                                const led89_part_desc *d)
{
    size_t next;

    led89_put_u64(out + off, d->file_id);
    led89_put_u64(out + off + 8u, d->first);
    led89_put_u64(out + off + 16u, d->end);
    next = led89_off_inc(off, (size_t)LED89_SEALED_DESC_SIZE);
    return next;
}

static int led89_decode_desc(const unsigned char *in, size_t *off,
                             led89_part_desc *d, led89_u64 *expected)
{
    d->file_id = led89_get_u64(in + *off);
    d->first = led89_get_u64(in + *off + 8u);
    d->end = led89_get_u64(in + *off + 16u);
    if (d->first != *expected)
    {
        return LEDGER89_ECORRUPT;
    }
    if (d->end <= d->first)
    {
        return LEDGER89_ECORRUPT;
    }
    *expected = d->end;
    *off = led89_off_inc(*off, (size_t)LED89_SEALED_DESC_SIZE);
    return LEDGER89_OK;
}

void led89_manifest_encode(unsigned char *out, const led89_manifest *m)
{
    size_t off;
    size_t i;
    size_t body;

    led89_manifest_encode_header(out, m);
    off = (size_t)LED89_MANIFEST_HEADER_SIZE;
    for (i = 0; i < (size_t)m->sealed_count; ++i)
    {
        off = led89_encode_desc(out, off, &m->sealed[i]);
    }
    led89_put_u64(out + off, m->active.file_id);
    led89_put_u64(out + off + 8u, m->active.first);
    off = led89_off_inc(off, (size_t)LED89_ACTIVE_DESC_SIZE);
    body = off;
    led89_put_u32(out + body, led89_crc32c(0u, out, body));
}

int led89_manifest_decode(const unsigned char *in, size_t size,
                          led89_manifest *m)
{
    led89_u32 count;
    size_t expect;
    size_t off;
    size_t i;
    led89_u64 prev_end;

    m->sealed = NULL;
    m->sealed_count = 0u;
    if (size < (size_t)LED89_MANIFEST_HEADER_SIZE +
                   (size_t)LED89_ACTIVE_DESC_SIZE +
                   (size_t)LED89_MANIFEST_CRC_SIZE)
    {
        return LEDGER89_ECORRUPT;
    }
    if (memcmp(in, LED89_MANIFEST_MAGIC, 8u) != 0)
    {
        return LEDGER89_ECORRUPT;
    }
    if (led89_get_u32(in + 52) != LED89_FORMAT_VERSION)
    {
        return LEDGER89_EFORMAT;
    }
    if (led89_get_u32(in + 56) != led89_crc32c(0u, in, 56u))
    {
        return LEDGER89_ECORRUPT;
    }
    count = led89_get_u32(in + 48);
    expect = led89_manifest_bytes(count);
    if (size != expect)
    {
        return LEDGER89_ECORRUPT;
    }
    if (led89_get_u32(in + size - 4u) != led89_crc32c(0u, in, size - 4u))
    {
        return LEDGER89_ECORRUPT;
    }
    m->generation = led89_get_u64(in + 8);
    memcpy(m->uuid, in + 16, 16u);
    m->revision = led89_get_u64(in + 32);
    m->first = led89_get_u64(in + 40);
    m->sealed_count = count;
    if (count > 0u)
    {
        m->sealed = led89_alloc_descs((size_t)count);
        if (m->sealed == NULL)
        {
            return LEDGER89_ENOMEM;
        }
    }
    off = (size_t)LED89_MANIFEST_HEADER_SIZE;
    prev_end = m->first;
    for (i = 0; i < (size_t)count; ++i)
    {
        int rc;

        rc = led89_decode_desc(in, &off, &m->sealed[i], &prev_end);
        if (rc != LEDGER89_OK)
        {
            led89_manifest_free(m);
            return rc;
        }
    }
    m->active.file_id = led89_get_u64(in + off);
    m->active.first = led89_get_u64(in + off + 8u);
    m->active.end = m->active.first;
    if (m->active.first != prev_end)
    {
        led89_manifest_free(m);
        return LEDGER89_ECORRUPT;
    }
    return LEDGER89_OK;
}

void led89_manifest_free(led89_manifest *m)
{
    free(m->sealed);
    m->sealed = NULL;
    m->sealed_count = 0u;
}

/* Part header ----------------------------------------------------------- */

void led89_part_header_encode(unsigned char *out, const led89_part_header *h)
{
    memset(out, 0, LED89_PART_HEADER_SIZE);
    memcpy(out, LED89_PART_MAGIC, 8u);
    memcpy(out + 8, h->uuid, 16u);
    led89_put_u64(out + 24, h->file_id);
    led89_put_u64(out + 32, h->revision);
    led89_put_u64(out + 40, h->first);
    led89_put_u32(out + 48, LED89_FORMAT_VERSION);
    led89_put_u32(out + 56, led89_crc32c(0u, out, 56u));
}

int led89_part_header_decode(const unsigned char *in, led89_part_header *h)
{
    if (memcmp(in, LED89_PART_MAGIC, 8u) != 0)
    {
        return LEDGER89_ECORRUPT;
    }
    if (led89_get_u32(in + 48) != LED89_FORMAT_VERSION)
    {
        return LEDGER89_EFORMAT;
    }
    if (led89_get_u32(in + 56) != led89_crc32c(0u, in, 56u))
    {
        return LEDGER89_ECORRUPT;
    }
    memcpy(h->uuid, in + 8, 16u);
    h->file_id = led89_get_u64(in + 24);
    h->revision = led89_get_u64(in + 32);
    h->first = led89_get_u64(in + 40);
    return LEDGER89_OK;
}

/* Batch header and footer ----------------------------------------------- */

void led89_batch_header_encode(unsigned char *out, const led89_batch_header *h)
{
    memset(out, 0, LED89_BATCH_HEADER_SIZE);
    memcpy(out, LED89_BATCH_HEADER_MAGIC, 4u);
    led89_put_u32(out + 4, h->count);
    led89_put_u64(out + 8, h->first);
    led89_put_u64(out + 16, h->bytes);
    led89_put_u32(out + 24, led89_crc32c(0u, out, 24u));
}

int led89_batch_header_decode(const unsigned char *in, led89_batch_header *h)
{
    if (!led89_is_batch_header(in))
    {
        return LEDGER89_ECORRUPT;
    }
    if (led89_get_u32(in + 24) != led89_crc32c(0u, in, 24u))
    {
        return LEDGER89_ECORRUPT;
    }
    h->count = led89_get_u32(in + 4);
    h->first = led89_get_u64(in + 8);
    h->bytes = led89_get_u64(in + 16);
    if (h->count == 0u)
    {
        return LEDGER89_ECORRUPT;
    }
    if (h->bytes < (led89_u64)LED89_MIN_BATCH_BYTES)
    {
        return LEDGER89_ECORRUPT;
    }
    return LEDGER89_OK;
}

void led89_batch_footer_encode(unsigned char *out, const led89_batch_footer *f)
{
    memset(out, 0, LED89_BATCH_FOOTER_SIZE);
    memcpy(out, LED89_BATCH_FOOTER_MAGIC, 4u);
    led89_put_u32(out + 4, f->count);
    led89_put_u64(out + 8, f->last);
}

int led89_batch_footer_decode(const unsigned char *in, led89_batch_footer *f)
{
    if (!led89_is_batch_footer(in))
    {
        return LEDGER89_ECORRUPT;
    }
    f->count = led89_get_u32(in + 4);
    f->last = led89_get_u64(in + 8);
    return LEDGER89_OK;
}

int led89_is_batch_header(const unsigned char *in)
{
    return memcmp(in, LED89_BATCH_HEADER_MAGIC, 4u) == 0;
}

int led89_is_batch_footer(const unsigned char *in)
{
    return memcmp(in, LED89_BATCH_FOOTER_MAGIC, 4u) == 0;
}

/* Stable marker --------------------------------------------------------- */

void led89_marker_encode(unsigned char *out, const led89_marker *m)
{
    memset(out, 0, LED89_MARKER_SIZE);
    memcpy(out, LED89_MARKER_MAGIC, 8u);
    memcpy(out + 8, m->uuid, 16u);
    led89_put_u64(out + 24, m->file_id);
    led89_put_u64(out + 32, m->revision);
    led89_put_u64(out + 40, m->end);
    led89_put_u32(out + 48, LED89_FORMAT_VERSION);
    led89_put_u32(out + 52, led89_crc32c(0u, out, 52u));
    memcpy(out + 56, LED89_MARKER_TRAILER, 8u);
}

int led89_marker_decode(const unsigned char *in, led89_marker *m)
{
    if (!led89_is_marker(in))
    {
        return LEDGER89_ECORRUPT;
    }
    if (led89_get_u32(in + 48) != LED89_FORMAT_VERSION)
    {
        return LEDGER89_EFORMAT;
    }
    if (led89_get_u32(in + 52) != led89_crc32c(0u, in, 52u))
    {
        return LEDGER89_ECORRUPT;
    }
    memcpy(m->uuid, in + 8, 16u);
    m->file_id = led89_get_u64(in + 24);
    m->revision = led89_get_u64(in + 32);
    m->end = led89_get_u64(in + 40);
    return LEDGER89_OK;
}

int led89_is_marker(const unsigned char *in)
{
    return memcmp(in, LED89_MARKER_MAGIC, 8u) == 0;
}

/* Sealed footer --------------------------------------------------------- */

void led89_sealed_footer_encode(unsigned char *out,
                                const led89_sealed_footer *f)
{
    memset(out, 0, LED89_SEALED_FOOTER_SIZE);
    memcpy(out, LED89_SEALED_FOOTER_MAGIC, 8u);
    memcpy(out + 8, f->uuid, 16u);
    led89_put_u64(out + 24, f->file_id);
    led89_put_u64(out + 32, f->first);
    led89_put_u64(out + 40, f->end);
    led89_put_u64(out + 48, f->records);
    led89_put_u32(out + 56, f->digest);
    led89_put_u32(out + 60, led89_crc32c(0u, out, 60u));
}

int led89_sealed_footer_decode(const unsigned char *in, led89_sealed_footer *f)
{
    if (memcmp(in, LED89_SEALED_FOOTER_MAGIC, 8u) != 0)
    {
        return LEDGER89_ECORRUPT;
    }
    if (led89_get_u32(in + 60) != led89_crc32c(0u, in, 60u))
    {
        return LEDGER89_ECORRUPT;
    }
    memcpy(f->uuid, in + 8, 16u);
    f->file_id = led89_get_u64(in + 24);
    f->first = led89_get_u64(in + 32);
    f->end = led89_get_u64(in + 40);
    f->records = led89_get_u64(in + 48);
    f->digest = led89_get_u32(in + 56);
    return LEDGER89_OK;
}
