#ifndef CKSUM89_H
#define CKSUM89_H

/*
 * cksum89.h - fixed checksum algorithms with streaming and one-shot
 * interfaces (ISO C89).
 *
 * Four algorithms:
 *
 *   CRC-32/ISO-HDLC   width 32, poly 0x04c11db7, init/xorout all ones,
 *                     reflected input and output
 *   CRC-32C           width 32, poly 0x1edc6f41, init/xorout all ones,
 *                     reflected input and output
 *   CRC-64/NVME       width 64, poly 0xad93d23594c93659, init/xorout all
 *                     ones, reflected input and output
 *   INET16            RFC 1071 16-bit one's-complement Internet checksum
 *
 * Each algorithm has one caller-owned context type and one
 * init/update/final triple; a one-shot wrapper performs exactly
 * init, update, final. The library allocates no memory, performs no
 * I/O, dispatches on no runtime state, and defines no error or status
 * vocabulary: every operation is total under the pointer preconditions
 * below.
 *
 * Portability:
 *
 *   - Requires ISO C90/C89.
 *   - Requires CHAR_BIT == 8.
 *   - Requires unsigned short to provide at least 16 value bits and
 *     unsigned long to provide at least 32 value bits; ISO C90
 *     guarantees both minimum ranges, so only CHAR_BIT is checked.
 *
 * cksum89_u16 and cksum89_u32 name exact VALUE DOMAINS stored in
 * potentially wider C integer types:
 *
 *   cksum89_u16: 0 .. 0xffff
 *   cksum89_u32: 0 .. 0xffffffff
 *
 * Values returned by this library never contain significant bits
 * outside those domains.
 */

#include <limits.h>
#include <stddef.h>

#if CHAR_BIT != 8
#error "libcksum89 requires 8-bit bytes"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Integer value types. These names describe the represented value
     * width, not necessarily sizeof(type) * CHAR_BIT. Callers compare
     * values numerically, not by object width.
     */
    typedef unsigned short cksum89_u16;
    typedef unsigned long cksum89_u32;

    /*
     * Unsigned 64-bit value represented as two 32-bit limbs.
     *
     * Numeric value: hi * 2^32 + lo. Both members contain values in
     * 0 .. 0xffffffff. This structure defines a numeric representation
     * only; its in-memory byte representation has no wire-format or
     * byte-order significance.
     */
    typedef struct cksum89_u64
    {
        cksum89_u32 hi;
        cksum89_u32 lo;
    } cksum89_u64;

    /*
     * Common pointer preconditions
     * ----------------------------
     *
     * For every function accepting a context pointer: ctx != NULL.
     *
     * For every function accepting (data, len):
     *
     *   len == 0:  data may be NULL;
     *   len != 0:  data points to at least len readable bytes.
     *
     * Passing an invalid pointer violates the API contract and
     * produces undefined behavior.
     *
     * update(ctx, NULL, 0) is valid and has no effect.
     *
     * Context types are value types. They require no special alignment
     * beyond that imposed by their C type, own no external resources,
     * and may be copied by ordinary structure assignment. Context
     * members are exposed for storage and copying only; applications
     * shall not inspect or modify individual members.
     *
     * final() does not modify its context. A context remains valid for
     * subsequent final() or update() calls.
     */

    /*
     * CRC-32 / ISO-HDLC
     *
     *   width   = 32
     *   poly    = 0x04c11db7
     *   init    = 0xffffffff
     *   refin   = true
     *   refout  = true
     *   xorout  = 0xffffffff
     *
     * check("123456789") = 0xcbf43926
     */
    typedef struct cksum89_crc32_iso_hdlc_ctx
    {
        cksum89_u32 state;
    } cksum89_crc32_iso_hdlc_ctx;

    void cksum89_crc32_iso_hdlc_init(cksum89_crc32_iso_hdlc_ctx *ctx);

    void cksum89_crc32_iso_hdlc_update(cksum89_crc32_iso_hdlc_ctx *ctx,
                                       const void *data, size_t len);

    cksum89_u32
    cksum89_crc32_iso_hdlc_final(const cksum89_crc32_iso_hdlc_ctx *ctx);

    cksum89_u32 cksum89_crc32_iso_hdlc(const void *data, size_t len);

    /*
     * CRC-32C / Castagnoli
     *
     *   width   = 32
     *   poly    = 0x1edc6f41
     *   init    = 0xffffffff
     *   refin   = true
     *   refout  = true
     *   xorout  = 0xffffffff
     *
     * check("123456789") = 0xe3069283
     */
    typedef struct cksum89_crc32c_ctx
    {
        cksum89_u32 state;
    } cksum89_crc32c_ctx;

    void cksum89_crc32c_init(cksum89_crc32c_ctx *ctx);

    void cksum89_crc32c_update(cksum89_crc32c_ctx *ctx, const void *data,
                               size_t len);

    cksum89_u32 cksum89_crc32c_final(const cksum89_crc32c_ctx *ctx);

    cksum89_u32 cksum89_crc32c(const void *data, size_t len);

    /*
     * CRC-64 / NVME
     *
     *   width   = 64
     *   poly    = 0xad93d23594c93659
     *   init    = 0xffffffffffffffff
     *   refin   = true
     *   refout  = true
     *   xorout  = 0xffffffffffffffff
     *
     * check("123456789") = 0xae8b14860a799888
     *
     * CRC-64 values use cksum89_u64 so that no C99 or
     * implementation-specific 64-bit integer type enters the public ABI.
     */
    typedef struct cksum89_crc64_nvme_ctx
    {
        cksum89_u64 state;
    } cksum89_crc64_nvme_ctx;

    void cksum89_crc64_nvme_init(cksum89_crc64_nvme_ctx *ctx);

    void cksum89_crc64_nvme_update(cksum89_crc64_nvme_ctx *ctx,
                                   const void *data, size_t len);

    cksum89_u64 cksum89_crc64_nvme_final(const cksum89_crc64_nvme_ctx *ctx);

    cksum89_u64 cksum89_crc64_nvme(const void *data, size_t len);

    /*
     * Internet checksum
     *
     * RFC 1071 16-bit one's-complement checksum. Input bytes form 16-bit
     * words in network order: word = byte[0] * 256 + byte[1]. A final
     * odd byte forms the high-order byte of the last word; its low-order
     * byte is zero. An update call may end between the two bytes of a
     * word; the context retains the pending byte.
     *
     * final() returns the one's complement of the accumulated sum as a
     * numeric value in 0 .. 0xffff. The function does not serialize that
     * value into network byte order.
     */
    typedef struct cksum89_inet16_ctx
    {
        cksum89_u32 sum;
        unsigned char pending;
        unsigned char has_pending;
    } cksum89_inet16_ctx;

    void cksum89_inet16_init(cksum89_inet16_ctx *ctx);

    void cksum89_inet16_update(cksum89_inet16_ctx *ctx, const void *data,
                               size_t len);

    cksum89_u16 cksum89_inet16_final(const cksum89_inet16_ctx *ctx);

    cksum89_u16 cksum89_inet16(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CKSUM89_H */
