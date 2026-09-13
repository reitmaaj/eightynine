/* test_crc.c - CRC-32C vectors and chunk independence. */

#include <string.h>

#include "test.h"

#include "ledger89_internal.h"

int main(void)
{
    const char *digits;
    unsigned char zeros[32];
    unsigned char data[64];
    led89_u32 one;
    led89_u32 split;
    size_t cut;
    size_t i;

    digits = "123456789";
    memset(zeros, 0, sizeof zeros);

    CHECK_EQ(led89_crc32c(0u, digits, 0u), 0x00000000u);
    CHECK_EQ(led89_crc32c(0u, digits, 9u), 0xE3069283u);
    CHECK_EQ(led89_crc32c(0u, zeros, sizeof zeros), 0x8A9136AAu);

    for (i = 0; i < sizeof data; ++i)
    {
        data[i] = (unsigned char)((i * 7u) + 3u);
    }
    one = led89_crc32c(0u, data, sizeof data);
    for (cut = 0u; cut <= sizeof data; ++cut)
    {
        split = led89_crc32c(0u, data, cut);
        split = led89_crc32c(split, data + cut, sizeof data - cut);
        CHECK_EQ(split, one);
    }

    TEST_END;
}
