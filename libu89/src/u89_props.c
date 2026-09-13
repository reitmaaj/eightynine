/* u89_props.c - UAX #31 / UCD binary properties: Default_Ignorable,
   Pattern_Syntax, Pattern_White_Space, Join_Control. */

#include "../include/u89.h"
#include "u89_tables.h"

/* Probe sorted, non-overlapping ranges. Returns 1 when cp lies in range i,
   2 when cp precedes range i (stop), otherwise 0 (continue). */
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

static int member(const u89_range *t, size_t n, u89_cp cp)
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

static int in_prop(const u89_range *t, size_t n, u89_cp cp)
{
    int sc;
    int m;

    sc = u89_is_scalar(cp);
    if (!sc)
    {
        return 0;
    }
    m = member(t, n, cp);
    return m;
}

int u89_default_ignorable(u89_cp cp)
{
    int r;

    r = in_prop(u89_defign_ranges, u89_defign_count, cp);
    return r;
}

int u89_pattern_whitespace(u89_cp cp)
{
    int r;

    r = in_prop(u89_patws_ranges, u89_patws_count, cp);
    return r;
}

int u89_pattern_syntax(u89_cp cp)
{
    int r;

    r = in_prop(u89_patsyn_ranges, u89_patsyn_count, cp);
    return r;
}

int u89_join_control(u89_cp cp)
{
    int r;

    r = in_prop(u89_join_ranges, u89_join_count, cp);
    return r;
}

/* Value lookup over sorted, non-overlapping u89_prop_range rows. Returns the
   stored value when cp lies in a row, else dflt. */
/* Probe one sorted u89_prop_range row. Returns 1 when cp lies in row i
   (writing its value to *out), 2 when cp precedes row i (stop), else 0. */
static int gc_probe(const u89_prop_range *t, size_t i, u89_cp cp,
                    unsigned short *out)
{
    if (cp < t[i].lo)
    {
        return 2;
    }
    if (cp > t[i].hi)
    {
        return 0;
    }
    *out = t[i].value;
    return 1;
}

static unsigned short prop_value(const u89_prop_range *t, size_t n, u89_cp cp,
                                 unsigned short dflt)
{
    size_t i;
    int st;
    unsigned short found;

    found = dflt;
    for (i = 0; i < n; ++i)
    {
        st = gc_probe(t, i, cp, &found);
        if (st == 2)
        {
            return dflt;
        }
        if (st == 1)
        {
            return found;
        }
    }
    return dflt;
}

int u89_general_category(u89_cp cp)
{
    unsigned short v;

    v = prop_value(u89_gc_ranges, u89_gc_count, cp, 0);
    return (int)v;
}

u89_eaw u89_east_asian_width(u89_cp cp)
{
    unsigned short v;

    v = prop_value(u89_eaw_ranges, u89_eaw_count, cp, 0);
    return (u89_eaw)v;
}

int u89_is_mark(u89_cp cp)
{
    int gc;
    int r;

    gc = u89_general_category(cp);
    r = 0;
    if (gc == U89_GC_Mn)
    {
        r = 1;
    }
    if (gc == U89_GC_Mc)
    {
        r = 1;
    }
    if (gc == U89_GC_Me)
    {
        r = 1;
    }
    return r;
}

int u89_is_control(u89_cp cp)
{
    int gc;

    gc = u89_general_category(cp);
    if (gc == U89_GC_Cc)
    {
        return 1;
    }
    return 0;
}

int u89_is_emoji(u89_cp cp)
{
    int r;

    r = in_prop(u89_emoji_ranges, u89_emoji_count, cp);
    return r;
}

int u89_is_emoji_presentation(u89_cp cp)
{
    int r;

    r = in_prop(u89_emoji_pres_ranges, u89_emoji_pres_count, cp);
    return r;
}
