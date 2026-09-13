/* ref_crc.c - test-only bit-at-a-time reference checksums.
 *
 * The CRCs implement the Rocksoft model directly from each algorithm's
 * normal polynomial: the register is processed MSB-first, input bytes are
 * reflected when RefIn is set, the final register is reflected when RefOut
 * is set, and XorOut is applied last. This is algorithmically independent
 * of the reflected table implementation under test.
 *
 * CRC-64 uses two 32-bit limbs so the whole suite stays strict C89.
 */

#include "ref_crc.h"

static unsigned char ref_reflect8(unsigned char value)
{
    unsigned char result;
    int bit;

    result = 0;
    for (bit = 0; bit < 8; ++bit)
    {
        result = (unsigned char)(((unsigned int)result << 1) |
                                 ((unsigned int)value & 1u));
        value = (unsigned char)(value >> 1);
    }
    return result;
}

static cksum89_u32 ref_reflect32(cksum89_u32 value)
{
    cksum89_u32 result;
    int bit;

    result = 0;
    for (bit = 0; bit < 32; ++bit)
    {
        result = (result << 1) | (value & 1UL);
        value = value >> 1;
    }
    return result;
}

static cksum89_u32 ref_crc32_rocksoft(cksum89_u32 poly, cksum89_u32 init,
                                      cksum89_u32 xorout,
                                      const unsigned char *data, size_t len)
{
    cksum89_u32 crc;
    size_t i;
    int bit;

    crc = init;
    for (i = 0; i < len; ++i)
    {
        crc = crc ^ ((cksum89_u32)ref_reflect8(data[i]) << 24);
        for (bit = 0; bit < 8; ++bit)
        {
            if ((crc & 0x80000000UL) != 0UL)
            {
                crc = ((crc << 1) ^ poly) & 0xffffffffUL;
            }
            else
            {
                crc = (crc << 1) & 0xffffffffUL;
            }
        }
    }
    return ref_reflect32(crc) ^ xorout;
}

cksum89_u32 ref_crc32_iso_hdlc(const unsigned char *data, size_t len)
{
    return ref_crc32_rocksoft(0x04c11db7UL, 0xffffffffUL, 0xffffffffUL, data,
                              len);
}

cksum89_u32 ref_crc32c(const unsigned char *data, size_t len)
{
    return ref_crc32_rocksoft(0x1edc6f41UL, 0xffffffffUL, 0xffffffffUL, data,
                              len);
}

cksum89_u32 ref_crc32_table_entry(cksum89_u32 poly, unsigned int index)
{
    cksum89_u32 crc;
    int bit;

    crc = (cksum89_u32)ref_reflect8((unsigned char)index) << 24;
    for (bit = 0; bit < 8; ++bit)
    {
        if ((crc & 0x80000000UL) != 0UL)
        {
            crc = ((crc << 1) ^ poly) & 0xffffffffUL;
        }
        else
        {
            crc = (crc << 1) & 0xffffffffUL;
        }
    }
    return ref_reflect32(crc);
}

static void ref_u64_shl1(cksum89_u64 *value)
{
    value->hi = ((value->hi << 1) | (value->lo >> 31)) & 0xffffffffUL;
    value->lo = (value->lo << 1) & 0xffffffffUL;
}

static void ref_u64_shr1(cksum89_u64 *value)
{
    value->lo = (value->lo >> 1) | ((value->hi & 1UL) << 31);
    value->hi = value->hi >> 1;
}

static cksum89_u64 ref_u64_reflect(cksum89_u64 value)
{
    cksum89_u64 result;
    int bit;

    result.hi = 0;
    result.lo = 0;
    for (bit = 0; bit < 64; ++bit)
    {
        ref_u64_shl1(&result);
        if ((value.lo & 1UL) != 0UL)
        {
            result.lo = result.lo | 1UL;
        }
        ref_u64_shr1(&value);
    }
    return result;
}

static cksum89_u64 ref_crc64_rocksoft(const unsigned char *data, size_t len)
{
    cksum89_u64 crc;
    cksum89_u64 poly;
    size_t i;
    int bit;

    crc.hi = 0xffffffffUL;
    crc.lo = 0xffffffffUL;
    poly.hi = 0xad93d235UL;
    poly.lo = 0x94c93659UL;
    for (i = 0; i < len; ++i)
    {
        crc.hi = crc.hi ^ ((cksum89_u32)ref_reflect8(data[i]) << 24);
        for (bit = 0; bit < 8; ++bit)
        {
            if ((crc.hi & 0x80000000UL) != 0UL)
            {
                ref_u64_shl1(&crc);
                crc.hi = crc.hi ^ poly.hi;
                crc.lo = crc.lo ^ poly.lo;
            }
            else
            {
                ref_u64_shl1(&crc);
            }
        }
    }
    crc = ref_u64_reflect(crc);
    crc.hi = crc.hi ^ 0xffffffffUL;
    crc.lo = crc.lo ^ 0xffffffffUL;
    return crc;
}

cksum89_u64 ref_crc64_nvme(const unsigned char *data, size_t len)
{
    return ref_crc64_rocksoft(data, len);
}

cksum89_u64 ref_crc64_table_entry(unsigned int index)
{
    cksum89_u64 crc;
    cksum89_u64 poly;
    int bit;

    crc.hi = (cksum89_u32)ref_reflect8((unsigned char)index) << 24;
    crc.lo = 0;
    poly.hi = 0xad93d235UL;
    poly.lo = 0x94c93659UL;
    for (bit = 0; bit < 8; ++bit)
    {
        if ((crc.hi & 0x80000000UL) != 0UL)
        {
            ref_u64_shl1(&crc);
            crc.hi = crc.hi ^ poly.hi;
            crc.lo = crc.lo ^ poly.lo;
        }
        else
        {
            ref_u64_shl1(&crc);
        }
    }
    return ref_u64_reflect(crc);
}

cksum89_u16 ref_inet16(const unsigned char *data, size_t len)
{
    cksum89_u32 sum;
    size_t i;

    sum = 0;
    i = 0;
    while (i + 1 < len)
    {
        sum = sum + (((cksum89_u32)data[i] << 8) | (cksum89_u32)data[i + 1]);
        sum = (sum & 0xffffUL) + (sum >> 16);
        i = i + 2;
    }
    if (i < len)
    {
        sum = sum + ((cksum89_u32)data[i] << 8);
        sum = (sum & 0xffffUL) + (sum >> 16);
    }
    return (cksum89_u16)(~sum & 0xffffUL);
}
