/* ledger89_crc.c - CRC-32C (Castagnoli) incremental checksum.
 *
 * Reflected polynomial 0x82F63B78, nibble table, seed 0 for a fresh
 * checksum. Chaining is transparent: the function finalizes on return, so
 * the result can be passed back as the next seed. */

#include "ledger89_internal.h"

static const led89_u32 led89_crc_table[16] = {
    0x00000000u, 0x105EC76Fu, 0x20BD8EDEu, 0x30E349B1u,
    0x417B1DBCu, 0x5125DAD3u, 0x61C69362u, 0x7198540Du,
    0x82F63B78u, 0x92A8FC17u, 0xA24BB5A6u, 0xB21572C9u,
    0xC38D26C4u, 0xD3D3E1ABu, 0xE330A81Au, 0xF36E6F75u};

static led89_u32 led89_crc_nibble(led89_u32 crc, led89_u32 nibble)
{
    led89_u32 index;

    index = (crc ^ nibble) & 0x0Fu;
    return (crc >> 4) ^ led89_crc_table[index];
}

static led89_u32 led89_crc_byte(led89_u32 crc, unsigned char byte)
{
    led89_u32 low;

    low = led89_crc_nibble(crc, byte & 0x0Fu);
    return led89_crc_nibble(low, (led89_u32)(byte >> 4));
}

led89_u32 led89_crc32c(led89_u32 crc, const void *data, size_t len)
{
    const unsigned char *p;
    size_t i;

    p = (const unsigned char *)data;
    crc = ~crc;
    for (i = 0; i < len; ++i)
    {
        crc = led89_crc_byte(crc, p[i]);
    }
    return ~crc;
}
