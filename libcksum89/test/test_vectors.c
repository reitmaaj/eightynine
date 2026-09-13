/* test_vectors.c - published vectors checked against the reference model. */

#include "ref_crc.h"
#include "test.h"

void test_vectors(void)
{
    static const unsigned char check[] = "123456789";
    static const unsigned char rfc[] = {0x00, 0x01, 0xf2, 0x03,
                                        0xf4, 0xf5, 0xf6, 0xf7};

    cksum89_test_u32(ref_crc32_iso_hdlc(NULL, 0), 0x00000000UL,
                     "reference crc32 iso empty");
    cksum89_test_u32(ref_crc32_iso_hdlc(check, 9), 0xcbf43926UL,
                     "reference crc32 iso check");
    cksum89_test_u32(ref_crc32c(NULL, 0), 0x00000000UL,
                     "reference crc32c empty");
    cksum89_test_u32(ref_crc32c(check, 9), 0xe3069283UL,
                     "reference crc32c check");
    cksum89_test_u64(ref_crc64_nvme(NULL, 0), 0x00000000UL, 0x00000000UL,
                     "reference crc64 nvme empty");
    cksum89_test_u64(ref_crc64_nvme(check, 9), 0xae8b1486UL, 0x0a799888UL,
                     "reference crc64 nvme check");
    cksum89_test_u16(ref_inet16(NULL, 0), 0xffffu, "reference inet16 empty");
    cksum89_test_u16(ref_inet16(check, 9), 0xf62au, "reference inet16 check");
    cksum89_test_u16(ref_inet16(rfc, 8), 0x220du, "reference inet16 rfc1071");
}
