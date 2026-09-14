/* ledger89_u64.c - portable 64-bit scalar helpers and internal arithmetic. */

#include "ledger89_internal.h"

int ledger89_u64_cmp(ledger89_u64 a, ledger89_u64 b)
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

int ledger89_u64_equal(ledger89_u64 a, ledger89_u64 b)
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

ledger89_u64 ledger89_u64_zero(void)
{
    ledger89_u64 zero;

    zero.hi = 0u;
    zero.lo = 0u;
    return zero;
}

ledger89_u64 ledger89_u64_from_u32(ledger89_u32 value)
{
    ledger89_u64 v;

    v.hi = 0u;
    v.lo = value;
    return v;
}

led89_u64 led89_from_public(ledger89_u64 v)
{
    return ((led89_u64)v.hi << 32) | (led89_u64)v.lo;
}

ledger89_u64 led89_to_public(led89_u64 v)
{
    ledger89_u64 out;

    out.hi = (ledger89_u32)(v >> 32);
    out.lo = (led89_u32)(v & (led89_u64)0xFFFFFFFFu);
    return out;
}

int led89_u64_add(led89_u64 a, led89_u64 b, led89_u64 *out)
{
    led89_u64 sum;

    sum = a + b;
    if (sum < a)
    {
        return 0;
    }
    *out = sum;
    return 1;
}

led89_u64 led89_u64_inc(led89_u64 a)
{
    return a + (led89_u64)1;
}

led89_u64 led89_u64_dec(led89_u64 a)
{
    return a - (led89_u64)1;
}

int led89_u64_cmp(led89_u64 a, led89_u64 b)
{
    if (a < b)
    {
        return -1;
    }
    if (a > b)
    {
        return 1;
    }
    return 0;
}

int led89_u64_is_zero(led89_u64 a)
{
    return a == (led89_u64)0;
}

int led89_u64_to_u32(led89_u64 v, led89_u32 *out)
{
    if (v > (led89_u64)0xFFFFFFFFu)
    {
        return LEDGER89_ERANGE;
    }
    *out = (led89_u32)v;
    return LEDGER89_OK;
}

int led89_u64_to_size(led89_u64 v, size_t *out)
{
    size_t s;

    s = (size_t)v;
    if ((led89_u64)s != v)
    {
        return LEDGER89_ERANGE;
    }
    *out = s;
    return LEDGER89_OK;
}

int led89_size_to_u64(size_t v, led89_u64 *out)
{
    *out = (led89_u64)v;
    return LEDGER89_OK;
}
