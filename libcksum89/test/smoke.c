/* smoke.c - one end-to-end path through all four algorithms: streaming
 * init/update/final plus the one-shot wrapper for each. */

#include <stdio.h>

#include "cksum89.h"

int main(void)
{
    static const unsigned char check[] = "123456789";
    cksum89_crc32_iso_hdlc_ctx iso;
    cksum89_crc32c_ctx crc32c;
    cksum89_crc64_nvme_ctx crc64;
    cksum89_inet16_ctx inet;
    cksum89_u64 value64;

    cksum89_crc32_iso_hdlc_init(&iso);
    cksum89_crc32_iso_hdlc_update(&iso, check, 9);
    if (cksum89_crc32_iso_hdlc_final(&iso) != 0xcbf43926UL)
    {
        fprintf(stderr, "smoke: crc32 iso streaming failed\n");
        return 1;
    }
    if (cksum89_crc32_iso_hdlc(check, 9) != 0xcbf43926UL)
    {
        fprintf(stderr, "smoke: crc32 iso one-shot failed\n");
        return 1;
    }

    cksum89_crc32c_init(&crc32c);
    cksum89_crc32c_update(&crc32c, check, 9);
    if (cksum89_crc32c_final(&crc32c) != 0xe3069283UL)
    {
        fprintf(stderr, "smoke: crc32c streaming failed\n");
        return 1;
    }
    if (cksum89_crc32c(check, 9) != 0xe3069283UL)
    {
        fprintf(stderr, "smoke: crc32c one-shot failed\n");
        return 1;
    }

    cksum89_crc64_nvme_init(&crc64);
    cksum89_crc64_nvme_update(&crc64, check, 9);
    value64 = cksum89_crc64_nvme_final(&crc64);
    if ((value64.hi != 0xae8b1486UL) || (value64.lo != 0x0a799888UL))
    {
        fprintf(stderr, "smoke: crc64 nvme streaming failed\n");
        return 1;
    }
    value64 = cksum89_crc64_nvme(check, 9);
    if ((value64.hi != 0xae8b1486UL) || (value64.lo != 0x0a799888UL))
    {
        fprintf(stderr, "smoke: crc64 nvme one-shot failed\n");
        return 1;
    }

    cksum89_inet16_init(&inet);
    cksum89_inet16_update(&inet, check, 9);
    if (cksum89_inet16_final(&inet) != 0xf62au)
    {
        fprintf(stderr, "smoke: inet16 streaming failed\n");
        return 1;
    }
    if (cksum89_inet16(check, 9) != 0xf62au)
    {
        fprintf(stderr, "smoke: inet16 one-shot failed\n");
        return 1;
    }

    printf("smoke: ok\n");
    return 0;
}
