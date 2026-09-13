#ifndef CKSUM89_INTERNAL_H
#define CKSUM89_INTERNAL_H

#include <stddef.h>

#include "cksum89.h"

/*
 * Internal helpers shared across libcksum89 translation units. Not public;
 * declared here so every definition is preceded by a prototype (green
 * requires -Wmissing-prototypes). The test suite reaches the checked-in
 * tables by including this header with -Isrc.
 *
 * The empty GREEN_PURE annotation lets green treat a following genuinely
 * pure function as callable in expression position.
 */
#define GREEN_PURE

/* Reflected byte-at-a-time tables, one per CRC algorithm. */
extern const cksum89_u32 cksum89_crc32_iso_hdlc_table[256];
extern const cksum89_u32 cksum89_crc32c_table[256];
extern const cksum89_u64 cksum89_crc64_nvme_table[256];

/* One reflected CRC-32 byte step against a selected table. */
GREEN_PURE
cksum89_u32 cksum89_crc32_step(cksum89_u32 state, const cksum89_u32 *table,
                               unsigned char byte);

/* Shift a two-limb value right by eight bits across the limb pair. */
GREEN_PURE
cksum89_u64 cksum89_crc64_shift8(cksum89_u64 value);

/* XOR two two-limb values. */
GREEN_PURE
cksum89_u64 cksum89_crc64_xor(cksum89_u64 a, cksum89_u64 b);

#endif /* CKSUM89_INTERNAL_H */
