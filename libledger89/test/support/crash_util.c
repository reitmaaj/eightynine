/* crash_util.c - shared helpers for the crash matrices. */

#include <string.h>

#include "crash_util.h"

static unsigned char cu_byte(ledger89_index index)
{
    return (unsigned char)('a' + (int)(index % 26ul));
}

int cu_open(mfs *fs, led89_io *io, ledger89 **l, unsigned long records)
{
    ledger89_config cfg;

    mfs_bind(io, fs);
    memset(&cfg, 0, sizeof cfg);
    cfg.path = "ledger";
    cfg.max_segment_bytes = 0ul;
    cfg.max_segment_records = records;
    *l = NULL;
    return led89_open_io(l, &cfg, io);
}

int cu_append(ledger89 *l, ledger89_index index)
{
    ledger89_record r;
    unsigned char value;

    value = cu_byte(index);
    r.index = index;
    r.tag = (unsigned long)index;
    r.data = &value;
    r.size = 1u;
    return ledger89_append(l, &r, 1u);
}

int cu_fill(ledger89 *l, ledger89_index count)
{
    ledger89_index i;

    for (i = 1ul; i <= count; ++i)
    {
        if (cu_append(l, i) != LEDGER89_OK)
        {
            return 0;
        }
    }
    return 1;
}

int cu_fill_range(ledger89 *l, ledger89_index first, ledger89_index last)
{
    ledger89_index i;

    for (i = first; i <= last; ++i)
    {
        if (cu_append(l, i) != LEDGER89_OK)
        {
            return 0;
        }
    }
    return 1;
}

int cu_check_range(ledger89 *l, ledger89_index first, ledger89_index last)
{
    ledger89_index i;

    if (first > last)
    {
        return 1;
    }
    for (i = first; i <= last; ++i)
    {
        ledger89_view v;

        if (ledger89_read(l, i, &v) != LEDGER89_OK)
        {
            return 0;
        }
        if (v.tag != (unsigned long)i)
        {
            return 0;
        }
        if (v.size != 1u)
        {
            return 0;
        }
        if (((const unsigned char *)v.data)[0] != cu_byte(i))
        {
            return 0;
        }
    }
    return 1;
}
