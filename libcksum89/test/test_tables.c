/* test_tables.c - every checked-in table entry against an independent
 * Rocksoft single-byte remainder. */

#include "cksum89_internal.h"
#include "ref_crc.h"
#include "test.h"

void test_tables(void)
{
    cksum89_u64 want;
    unsigned int i;

    for (i = 0; i < 256u; ++i)
    {
        cksum89_test_u32(cksum89_crc32_iso_hdlc_table[i],
                         ref_crc32_table_entry(0x04c11db7UL, i),
                         "crc32 iso table entry");
        cksum89_test_u32(cksum89_crc32c_table[i],
                         ref_crc32_table_entry(0x1edc6f41UL, i),
                         "crc32c table entry");
        want = ref_crc64_table_entry(i);
        cksum89_test_u64(cksum89_crc64_nvme_table[i], want.hi, want.lo,
                         "crc64 nvme table entry");
    }
}
