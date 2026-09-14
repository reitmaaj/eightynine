/* syntax89_gen.c - deterministic xorshift64 generator. */

#include "syntax89_gen.h"

void syntax89_rng_seed(struct syntax89_rng *r, unsigned long seed)
{
    r->state = seed;
    if (r->state == 0)
    {
        r->state = 0x9E3779B97F4A7C15UL;
    }
}

unsigned long syntax89_rng_next(struct syntax89_rng *r)
{
    unsigned long x;

    x = r->state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    r->state = x;
    return x;
}

unsigned long syntax89_rng_below(struct syntax89_rng *r, unsigned long n)
{
    if (n == 0)
    {
        return 0;
    }
    return syntax89_rng_next(r) % n;
}
