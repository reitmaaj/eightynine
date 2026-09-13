// header_check.cpp - the public header compiles under C++23 and keeps its
// C ABI shape.

#include <cstddef>

#include "cksum89.h"

static_assert(sizeof(cksum89_u64) == 2 * sizeof(cksum89_u32),
              "cksum89_u64 layout");
static_assert(sizeof(cksum89_crc32_iso_hdlc_ctx) == sizeof(cksum89_u32),
              "cksum89_crc32_iso_hdlc_ctx layout");
static_assert(sizeof(cksum89_crc32c_ctx) == sizeof(cksum89_u32),
              "cksum89_crc32c_ctx layout");
static_assert(sizeof(cksum89_crc64_nvme_ctx) == sizeof(cksum89_u64),
              "cksum89_crc64_nvme_ctx layout");

int main()
{
    cksum89_crc32c_ctx ctx;

    cksum89_crc32c_init(&ctx);
    if (cksum89_crc32c_final(&ctx) == 0UL)
    {
        return 0;
    }
    return 1;
}
