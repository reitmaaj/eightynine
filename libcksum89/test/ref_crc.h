#ifndef REF_CRC_H
#define REF_CRC_H

/*
 * ref_crc.h - test-only bit-at-a-time reference checksums.
 *
 * The CRCs implement the Rocksoft model directly from each algorithm's
 * normal polynomial: the register is processed MSB-first, input bytes are
 * reflected when RefIn is set, the final register is reflected when RefOut
 * is set, and XorOut is applied last. This is algorithmically independent
 * of the reflected table implementation under test.
 */

#include <stddef.h>

#include "cksum89.h"

cksum89_u32 ref_crc32_iso_hdlc(const unsigned char *data, size_t len);
cksum89_u32 ref_crc32c(const unsigned char *data, size_t len);
cksum89_u64 ref_crc64_nvme(const unsigned char *data, size_t len);
cksum89_u16 ref_inet16(const unsigned char *data, size_t len);

/* Single-byte Rocksoft remainder for the reflected production table. */
cksum89_u32 ref_crc32_table_entry(cksum89_u32 poly, unsigned int index);
cksum89_u64 ref_crc64_table_entry(unsigned int index);

#endif /* REF_CRC_H */
