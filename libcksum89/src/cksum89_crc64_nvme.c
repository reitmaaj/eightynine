/* cksum89_crc64_nvme.c - CRC-64/NVMe streaming and one-shot.
 *
 * Reflected polynomial 0x9a6c9329ac4bc9b5 with all-ones init and xorout,
 * represented as two 32-bit limbs. The byte-at-a-time step indexes the
 * checked-in 256-entry table; no native 64-bit integer operation enters
 * the implementation.
 */

#include "cksum89_internal.h"

GREEN_PURE
cksum89_u64 cksum89_crc64_shift8(cksum89_u64 value)
{
    cksum89_u64 result;

    result.lo = (value.lo >> 8) | ((value.hi & 0xffUL) << 24);
    result.hi = value.hi >> 8;
    return result;
}

GREEN_PURE
cksum89_u64 cksum89_crc64_xor(cksum89_u64 a, cksum89_u64 b)
{
    cksum89_u64 result;

    result.hi = a.hi ^ b.hi;
    result.lo = a.lo ^ b.lo;
    return result;
}

static void cksum89_crc64_nvme_byte(cksum89_crc64_nvme_ctx *ctx,
                                    unsigned char byte)
{
    cksum89_u32 index;
    cksum89_u64 shifted;

    index = (ctx->state.lo ^ (cksum89_u32)byte) & 0xffUL;
    shifted = cksum89_crc64_shift8(ctx->state);
    ctx->state = cksum89_crc64_xor(shifted, cksum89_crc64_nvme_table[index]);
}

void cksum89_crc64_nvme_init(cksum89_crc64_nvme_ctx *ctx)
{
    ctx->state.hi = 0xffffffffUL;
    ctx->state.lo = 0xffffffffUL;
}

void cksum89_crc64_nvme_update(cksum89_crc64_nvme_ctx *ctx, const void *data,
                               size_t len)
{
    const unsigned char *bytes;
    size_t i;

    bytes = (const unsigned char *)data;
    for (i = 0; i < len; ++i)
    {
        cksum89_crc64_nvme_byte(ctx, bytes[i]);
    }
}

GREEN_PURE
cksum89_u64 cksum89_crc64_nvme_final(const cksum89_crc64_nvme_ctx *ctx)
{
    cksum89_u64 result;

    result.hi = ctx->state.hi ^ 0xffffffffUL;
    result.lo = ctx->state.lo ^ 0xffffffffUL;
    return result;
}

cksum89_u64 cksum89_crc64_nvme(const void *data, size_t len)
{
    cksum89_crc64_nvme_ctx ctx;
    cksum89_u64 result;

    cksum89_crc64_nvme_init(&ctx);
    cksum89_crc64_nvme_update(&ctx, data, len);
    result = cksum89_crc64_nvme_final(&ctx);
    return result;
}
