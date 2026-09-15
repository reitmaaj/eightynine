/* raft89_u64.c - portable 64-bit scalar helpers and internal arithmetic. */

#include "raft89_internal.h"

int raft89_u64_cmp(raft89_u64 a, raft89_u64 b)
{
    if (a.hi < b.hi)
    {
        return -1;
    }
    if (a.hi > b.hi)
    {
        return 1;
    }
    if (a.lo < b.lo)
    {
        return -1;
    }
    if (a.lo > b.lo)
    {
        return 1;
    }
    return 0;
}

int raft89_u64_equal(raft89_u64 a, raft89_u64 b)
{
    if (a.hi != b.hi)
    {
        return 0;
    }
    if (a.lo != b.lo)
    {
        return 0;
    }
    return 1;
}

raft89_u64 raft89_u64_zero(void)
{
    raft89_u64 zero;
    zero.hi = 0u;
    zero.lo = 0u;
    return zero;
}

raft89_u64 raft89_u64_from_u32(raft89_u32 value)
{
    raft89_u64 v;
    v.hi = 0u;
    v.lo = value;
    return v;
}

raft89__u64 raft89__from_public(raft89_u64 v)
{
    return ((raft89__u64)v.hi << 32) | (raft89__u64)v.lo;
}

raft89_u64 raft89__to_public(raft89__u64 v)
{
    raft89_u64 out;
    out.hi = (raft89_u32)(v >> 32);
    out.lo = (raft89_u32)(v & (raft89__u64)0xFFFFFFFFu);
    return out;
}

int raft89__u64_is_zero(raft89__u64 v)
{
    return v == (raft89__u64)0;
}

int raft89__u64_add(raft89__u64 a, raft89__u64 b, raft89__u64 *out)
{
    raft89__u64 sum;
    sum = a + b;
    if (sum < a)
    {
        return 0;
    }
    *out = sum;
    return 1;
}

raft89__u64 raft89__u64_inc(raft89__u64 a)
{
    return a + (raft89__u64)1;
}

raft89__u64 raft89__u64_dec(raft89__u64 a)
{
    return a - (raft89__u64)1;
}

int raft89__u64_to_size(raft89__u64 v, raft89_size *out)
{
    if (v > (raft89__u64)ULONG_MAX)
    {
        return 0;
    }
    *out = (raft89_size)v;
    return 1;
}

raft89__u64 raft89__size_to_u64(raft89_size v)
{
    return (raft89__u64)v;
}
