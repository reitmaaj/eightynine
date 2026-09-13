/* cksum89_crc32_iso_hdlc.c - CRC-32/ISO-HDLC streaming and one-shot.
 *
 * Reflected polynomial 0xedb88320 with all-ones init and xorout. The
 * byte-at-a-time table step is shared with CRC-32C through
 * cksum89_crc32_step.
 */

#include "cksum89_internal.h"

GREEN_PURE
cksum89_u32 cksum89_crc32_step(cksum89_u32 state, const cksum89_u32 *table,
                               unsigned char byte)
{
    cksum89_u32 index;

    index = (state ^ (cksum89_u32)byte) & 0xffUL;
    return (state >> 8) ^ table[index];
}

static void cksum89_crc32_iso_hdlc_byte(cksum89_crc32_iso_hdlc_ctx *ctx,
                                        unsigned char byte)
{
    ctx->state =
        cksum89_crc32_step(ctx->state, cksum89_crc32_iso_hdlc_table, byte);
}

void cksum89_crc32_iso_hdlc_init(cksum89_crc32_iso_hdlc_ctx *ctx)
{
    ctx->state = 0xffffffffUL;
}

void cksum89_crc32_iso_hdlc_update(cksum89_crc32_iso_hdlc_ctx *ctx,
                                   const void *data, size_t len)
{
    const unsigned char *bytes;
    size_t i;

    bytes = (const unsigned char *)data;
    for (i = 0; i < len; ++i)
    {
        cksum89_crc32_iso_hdlc_byte(ctx, bytes[i]);
    }
}

GREEN_PURE
cksum89_u32 cksum89_crc32_iso_hdlc_final(const cksum89_crc32_iso_hdlc_ctx *ctx)
{
    return ctx->state ^ 0xffffffffUL;
}

cksum89_u32 cksum89_crc32_iso_hdlc(const void *data, size_t len)
{
    cksum89_crc32_iso_hdlc_ctx ctx;
    cksum89_u32 result;

    cksum89_crc32_iso_hdlc_init(&ctx);
    cksum89_crc32_iso_hdlc_update(&ctx, data, len);
    result = cksum89_crc32_iso_hdlc_final(&ctx);
    return result;
}
