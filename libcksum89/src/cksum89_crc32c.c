/* cksum89_crc32c.c - CRC-32C (Castagnoli) streaming and one-shot.
 *
 * Reflected polynomial 0x82f63b78 with all-ones init and xorout. The
 * byte-at-a-time table step is shared with CRC-32/ISO-HDLC through
 * cksum89_crc32_step.
 */

#include "cksum89_internal.h"

static void cksum89_crc32c_byte(cksum89_crc32c_ctx *ctx, unsigned char byte)
{
    ctx->state = cksum89_crc32_step(ctx->state, cksum89_crc32c_table, byte);
}

void cksum89_crc32c_init(cksum89_crc32c_ctx *ctx)
{
    ctx->state = 0xffffffffUL;
}

void cksum89_crc32c_update(cksum89_crc32c_ctx *ctx, const void *data,
                           size_t len)
{
    const unsigned char *bytes;
    size_t i;

    bytes = (const unsigned char *)data;
    for (i = 0; i < len; ++i)
    {
        cksum89_crc32c_byte(ctx, bytes[i]);
    }
}

GREEN_PURE
cksum89_u32 cksum89_crc32c_final(const cksum89_crc32c_ctx *ctx)
{
    return ctx->state ^ 0xffffffffUL;
}

cksum89_u32 cksum89_crc32c(const void *data, size_t len)
{
    cksum89_crc32c_ctx ctx;
    cksum89_u32 result;

    cksum89_crc32c_init(&ctx);
    cksum89_crc32c_update(&ctx, data, len);
    result = cksum89_crc32c_final(&ctx);
    return result;
}
