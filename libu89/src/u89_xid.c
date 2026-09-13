/* u89_xid.c - UAX #31 XID_Start / XID_Continue predicates. */

#include "../include/u89.h"
#include "u89_tables.h"

/* Probe sorted, non-overlapping ranges. Returns 1 when cp lies in range i,
   2 when cp precedes range i (ranges are ascending, so scanning may stop),
   otherwise 0 (continue scanning). */
static int range_probe(const u89_range *t, size_t i, u89_cp cp);

static int range_probe(const u89_range *t, size_t i, u89_cp cp)
{
    u89_cp tlo;
    u89_cp thi;

    tlo = t[i].lo;
    if (cp < tlo)
    {
        return 2;
    }
    thi = t[i].hi;
    if (cp <= thi)
    {
        return 1;
    }
    return 0;
}

static int in_ranges(const u89_range *t, size_t n, u89_cp cp)
{
    size_t i;
    int st;

    for (i = 0; i < n; ++i)
    {
        st = range_probe(t, i, cp);
        if (st == 1)
        {
            return 1;
        }
        if (st == 2)
        {
            return 0;
        }
    }
    return 0;
}

int u89_xid_start(u89_cp cp)
{
    int sc;
    int r;

    sc = u89_is_scalar(cp);
    if (!sc)
    {
        return 0;
    }
    r = in_ranges(u89_xid_start_ranges, u89_xid_start_count, cp);
    return r;
}

int u89_xid_continue(u89_cp cp)
{
    int sc;
    int r;

    sc = u89_is_scalar(cp);
    if (!sc)
    {
        return 0;
    }
    r = in_ranges(u89_xid_cont_ranges, u89_xid_cont_count, cp);
    return r;
}
