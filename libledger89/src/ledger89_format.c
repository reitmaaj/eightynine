/* ledger89_format.c - fixed little-endian structure codecs.
 *
 * Every field is serialized explicitly; no native structure is ever written
 * to disk. Decoders validate magic, reserved bits, ranges, and self-checksums
 * before returning any field. The batch CRC and record CRC cover variable
 * data and are validated by their callers. */

#include <string.h>

#include "ledger89_internal.h"

static const unsigned char led89_seg_magic[8] = {'L', 'D', '8', '9',
                                                 'S', 'E', 'G', '1'};
static const unsigned char led89_rec_magic[4] = {'R', '8', '9', 0x01};
static const unsigned char led89_batch_magic[4] = {'B', '8', '9', 0x01};
static const unsigned char led89_batch_end_magic[4] = {'b', '8', '9', 0x01};
static const unsigned char led89_seg_end_magic[8] = {'L', 'D', '8', '9',
                                                     'E', 'N', 'D', '1'};

int led89_is_batch_header(const unsigned char *in)
{
    return led89_bytes_equal(in, led89_batch_magic, 4u);
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
    out[0] = (unsigned char)(v & 0xFFu);
    out[1] = (unsigned char)((v >> 8) & 0xFFu);
    out[2] = (unsigned char)((v >> 16) & 0xFFu);
    out[3] = (unsigned char)((v >> 24) & 0xFFu);
    out[4] = (unsigned char)((v >> 32) & 0xFFu);
    out[5] = (unsigned char)((v >> 40) & 0xFFu);
    out[6] = (unsigned char)((v >> 48) & 0xFFu);
    out[7] = (unsigned char)((v >> 56) & 0xFFu);
}

led89_u16 led89_get_u16(const unsigned char *in)
{
    led89_u16 v;

    v = (led89_u16)in[0];
    v = (led89_u16)(v | (led89_u16)((led89_u16)in[1] << 8));
    return v;
}

led89_u32 led89_get_u32(const unsigned char *in)
{
    led89_u32 v;

    v = (led89_u32)in[0];
    v = v | ((led89_u32)in[1] << 8);
    v = v | ((led89_u32)in[2] << 16);
    v = v | ((led89_u32)in[3] << 24);
    return v;
}

led89_u64 led89_get_u64(const unsigned char *in)
{
    led89_u64 v;

    v = (led89_u64)in[0];
    v = v | ((led89_u64)in[1] << 8);
    v = v | ((led89_u64)in[2] << 16);
    v = v | ((led89_u64)in[3] << 24);
    v = v | ((led89_u64)in[4] << 32);
    v = v | ((led89_u64)in[5] << 40);
    v = v | ((led89_u64)in[6] << 48);
    v = v | ((led89_u64)in[7] << 56);
    return v;
}

void led89_seg_header_encode(unsigned char *out, const led89_seg_header *h)
{
    memcpy(out, led89_seg_magic, 8u);
    led89_put_u16(out + 8, LED89_FORMAT_VERSION);
    led89_put_u16(out + 10, (led89_u16)LED89_SEGMENT_HEADER_SIZE);
    led89_put_u32(out + 12, h->flags);
    led89_put_u64(out + 16, h->first_index);
    led89_put_u32(out + 24, 0u);
    led89_put_u32(out + 28, led89_crc32c(0u, out, 28u));
}

int led89_seg_header_decode(const unsigned char *in, led89_seg_header *h)
{
    if (led89_bytes_equal(in, led89_seg_magic, 8u) == 0)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (led89_get_u16(in + 8) != LED89_FORMAT_VERSION)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (led89_get_u16(in + 10) != (led89_u16)LED89_SEGMENT_HEADER_SIZE)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (led89_get_u32(in + 28) != led89_crc32c(0u, in, 28u))
    {
        return LEDGER89_ERR_CORRUPT;
    }
    h->first_index = led89_get_u64(in + 16);
    h->flags = led89_get_u32(in + 12);
    return LEDGER89_OK;
}

void led89_rec_header_encode(unsigned char *out, const led89_rec_header *h)
{
    memcpy(out, led89_rec_magic, 4u);
    led89_put_u32(out + 4, h->flags);
    led89_put_u64(out + 8, h->index);
    led89_put_u64(out + 16, h->tag);
    led89_put_u32(out + 24, h->payload_size);
    led89_put_u32(out + 28, 0u);
}

int led89_rec_header_decode(const unsigned char *in, led89_rec_header *h)
{
    if (led89_bytes_equal(in, led89_rec_magic, 4u) == 0)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (led89_get_u32(in + 4) != 0u)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (led89_get_u32(in + 28) != 0u)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (led89_get_u32(in + 24) > LEDGER89_MAX_RECORD_BYTES)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    h->index = led89_get_u64(in + 8);
    h->tag = led89_get_u64(in + 16);
    h->payload_size = led89_get_u32(in + 24);
    h->flags = 0u;
    return LEDGER89_OK;
}

void led89_batch_header_encode(unsigned char *out, const led89_batch_header *h)
{
    memcpy(out, led89_batch_magic, 4u);
    led89_put_u32(out + 4, h->record_count);
    led89_put_u64(out + 8, h->first_index);
    led89_put_u64(out + 16, h->batch_bytes);
}

int led89_batch_header_decode(const unsigned char *in, led89_batch_header *h)
{
    if (led89_bytes_equal(in, led89_batch_magic, 4u) == 0)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    h->record_count = led89_get_u32(in + 4);
    h->first_index = led89_get_u64(in + 8);
    h->batch_bytes = led89_get_u64(in + 16);
    if (h->record_count == 0u)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (h->batch_bytes < (led89_u64)LED89_MIN_BATCH_BYTES)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    return LEDGER89_OK;
}

void led89_batch_footer_encode(unsigned char *out, const led89_batch_footer *f)
{
    memcpy(out, led89_batch_end_magic, 4u);
    led89_put_u32(out + 4, f->record_count);
    led89_put_u64(out + 8, f->last_index);
    led89_put_u32(out + 16, f->crc);
    led89_put_u32(out + 20, 0u);
}

int led89_batch_footer_decode(const unsigned char *in, led89_batch_footer *f)
{
    if (led89_bytes_equal(in, led89_batch_end_magic, 4u) == 0)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    f->record_count = led89_get_u32(in + 4);
    f->last_index = led89_get_u64(in + 8);
    f->crc = led89_get_u32(in + 16);
    if (led89_get_u32(in + 20) != 0u)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    return LEDGER89_OK;
}

void led89_seg_footer_encode(unsigned char *out, const led89_seg_footer *f)
{
    memcpy(out, led89_seg_end_magic, 8u);
    led89_put_u64(out + 8, f->last_index);
    led89_put_u64(out + 16, f->record_count);
    led89_put_u32(out + 24, f->segment_digest);
    led89_put_u32(out + 28, 0u);
    led89_put_u64(out + 32, f->body_size);
    led89_put_u32(out + 40, led89_crc32c(0u, out, 40u));
    led89_put_u32(out + 44, 0u);
}

int led89_seg_footer_decode(const unsigned char *in, led89_seg_footer *f)
{
    if (led89_bytes_equal(in, led89_seg_end_magic, 8u) == 0)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (led89_get_u32(in + 28) != 0u)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (led89_get_u32(in + 44) != 0u)
    {
        return LEDGER89_ERR_CORRUPT;
    }
    if (led89_get_u32(in + 40) != led89_crc32c(0u, in, 40u))
    {
        return LEDGER89_ERR_CORRUPT;
    }
    f->last_index = led89_get_u64(in + 8);
    f->record_count = led89_get_u64(in + 16);
    f->segment_digest = led89_get_u32(in + 24);
    f->body_size = led89_get_u64(in + 32);
    return LEDGER89_OK;
}
