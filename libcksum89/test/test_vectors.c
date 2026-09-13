/* test_vectors.c - published vectors checked against the reference model. */

#include "ref_crc.h"
#include "test.h"

void test_vectors(void)
{
    static const unsigned char check[] = "123456789";
    static const unsigned char rfc[] = {0x00, 0x01, 0xf2, 0x03,
                                        0xf4, 0xf5, 0xf6, 0xf7};
    unsigned char zeros32[32];
    unsigned char ones32[32];
    unsigned char inc32[32];
    size_t i;

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

    cksum89_test_u32(cksum89_crc32_iso_hdlc(NULL, 0), 0x00000000UL,
                     "crc32 iso empty");
    cksum89_test_u32(cksum89_crc32_iso_hdlc(check, 9), 0xcbf43926UL,
                     "crc32 iso check");

    for (i = 0; i < 32; ++i)
    {
        zeros32[i] = 0;
        ones32[i] = 0xff;
        inc32[i] = (unsigned char)i;
    }
    cksum89_test_u32(cksum89_crc32c(NULL, 0), 0x00000000UL, "crc32c empty");
    cksum89_test_u32(cksum89_crc32c(check, 9), 0xe3069283UL, "crc32c check");
    cksum89_test_u32(cksum89_crc32c(zeros32, 32), 0x8a9136aaUL,
                     "crc32c iscsi zero32");
    cksum89_test_u32(cksum89_crc32c(ones32, 32), 0x62a8ab43UL,
                     "crc32c iscsi ones32");
    cksum89_test_u32(cksum89_crc32c(inc32, 32), 0x46dd794eUL,
                     "crc32c iscsi incrementing32");
}
