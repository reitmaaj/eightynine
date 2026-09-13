/* prng.c - deterministic 32-bit linear congruential generator. */
#include "prng.h"

void prng_seed(prng *p, unsigned long seed)
{
    p->state = (seed ^ 0x9E3779B9ul) & 0xFFFFFFFFul;
    if (p->state == 0ul)
    {
        p->state = 0x2545F491ul;
    }
}

unsigned long prng_next(prng *p)
{
    p->state = (p->state * 1103515245ul + 12345ul) & 0xFFFFFFFFul;
    return p->state;
}

unsigned long prng_below(prng *p, unsigned long bound)
{
    if (bound == 0ul)
    {
        return 0ul;
    }
    return prng_next(p) % bound;
}
