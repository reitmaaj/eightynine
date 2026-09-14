/* crash_util.c - shared helpers for the crash matrices. */

#include <string.h>

#include "crash_util.h"

static unsigned char cu_byte(unsigned long index)
{
    return (unsigned char)('a' + (int)(index % 26ul));
}

int cu_open(mfs *fs, led89_io *io, ledger89 **l)
{
    mfs_bind(io, fs);
    *l = NULL;
    return led89_open_io(l, "ledger", LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE,
                         io);
}

int cu_reopen(mfs *fs, led89_io *io, ledger89 **l, ledger89_state *st)
{
    *l = NULL;
    if (cu_open(fs, io, l) != LEDGER89_OK)
    {
        return 0;
    }
    if (ledger89_get_state(*l, st) != LEDGER89_OK)
    {
        return 0;
    }
    return 1;
}

void cu_set_target(ledger89 *l, unsigned long bytes)
{
    l->part_target = (led89_u64)bytes;
}

int cu_append(ledger89 *l, unsigned long value)
{
    unsigned char byte;
    ledger89_slice s;

    byte = cu_byte(value);
    s.data = &byte;
    s.size = 1u;
    return ledger89_appendv(l, &s, 1u, NULL);
}

int cu_fill(ledger89 *l, unsigned long count)
{
    return cu_fill_range(l, 1ul, count);
}

int cu_fill_range(ledger89 *l, unsigned long first, unsigned long last)
{
    unsigned long i;

    for (i = first; i <= last; ++i)
    {
        if (cu_append(l, i) != LEDGER89_OK)
        {
            return 0;
        }
    }
    return 1;
}

int cu_check_range(ledger89 *l, unsigned long first, unsigned long last)
{
    unsigned long i;

    for (i = first; i <= last; ++i)
    {
        unsigned char buf[1];
        size_t size;
        ledger89_index index;

        index = ledger89_u64_from_u32((ledger89_u32)i);
        if (ledger89_read(l, index, buf, sizeof buf, &size) != LEDGER89_OK)
        {
            return 0;
        }
        if (size != 1u)
        {
            return 0;
        }
        if (buf[0] != cu_byte(i))
        {
            return 0;
        }
    }
    return 1;
}
