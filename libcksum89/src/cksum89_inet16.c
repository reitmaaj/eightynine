/* cksum89_inet16.c - RFC 1071 16-bit one's-complement Internet checksum.
 *
 * Input octets form big-endian 16-bit words. An update may end between the
 * two octets of a word; the context retains the pending octet and pairs it
 * with the first octet of the next update. Only final treats a remaining
 * octet as [octet, 0x00]. The accumulator folds end-around carry after
 * every word, so an arbitrarily long stream cannot overflow.
 */

#include "cksum89_internal.h"

GREEN_PURE
static cksum89_u32 cksum89_inet16_fold(cksum89_u32 sum)
{
    cksum89_u32 folded;

    folded = (sum & 0xffffUL) + (sum >> 16);
    return (folded & 0xffffUL) + (folded >> 16);
}

GREEN_PURE
static cksum89_u32 cksum89_inet16_pending_word(const cksum89_inet16_ctx *ctx)
{
    return (cksum89_u32)ctx->pending << 8;
}

static void cksum89_inet16_add_word(cksum89_inet16_ctx *ctx, cksum89_u32 word)
{
    ctx->sum = cksum89_inet16_fold(ctx->sum + word);
}

static void cksum89_inet16_pair(cksum89_inet16_ctx *ctx, unsigned char byte)
{
    cksum89_u32 word;

    word = cksum89_inet16_pending_word(ctx) | (cksum89_u32)byte;
    cksum89_inet16_add_word(ctx, word);
    ctx->has_pending = 0;
}

static void cksum89_inet16_hold(cksum89_inet16_ctx *ctx, unsigned char byte)
{
    ctx->pending = byte;
    ctx->has_pending = 1;
}

static void cksum89_inet16_byte(cksum89_inet16_ctx *ctx, unsigned char byte)
{
    if (ctx->has_pending)
    {
        cksum89_inet16_pair(ctx, byte);
    }
    else
    {
        cksum89_inet16_hold(ctx, byte);
    }
}

void cksum89_inet16_init(cksum89_inet16_ctx *ctx)
{
    ctx->sum = 0;
    ctx->pending = 0;
    ctx->has_pending = 0;
}

void cksum89_inet16_update(cksum89_inet16_ctx *ctx, const void *data,
                           size_t len)
{
    const unsigned char *bytes;
    size_t i;

    bytes = (const unsigned char *)data;
    for (i = 0; i < len; ++i)
    {
        cksum89_inet16_byte(ctx, bytes[i]);
    }
}

GREEN_PURE
static cksum89_u32 cksum89_inet16_sum_pending(const cksum89_inet16_ctx *ctx)
{
    return cksum89_inet16_fold(ctx->sum + cksum89_inet16_pending_word(ctx));
}

GREEN_PURE
static cksum89_u32 cksum89_inet16_total(const cksum89_inet16_ctx *ctx)
{
    cksum89_u32 total;

    total = ctx->sum;
    if (ctx->has_pending)
    {
        total = cksum89_inet16_sum_pending(ctx);
    }
    return total;
}

GREEN_PURE
cksum89_u16 cksum89_inet16_final(const cksum89_inet16_ctx *ctx)
{
    return (cksum89_u16)(~cksum89_inet16_fold(cksum89_inet16_total(ctx)) &
                         0xffffUL);
}

cksum89_u16 cksum89_inet16(const void *data, size_t len)
{
    cksum89_inet16_ctx ctx;
    cksum89_u16 result;

    cksum89_inet16_init(&ctx);
    cksum89_inet16_update(&ctx, data, len);
    result = cksum89_inet16_final(&ctx);
    return result;
}
