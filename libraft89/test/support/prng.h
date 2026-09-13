#ifndef PRNG_H
#define PRNG_H

/* prng.h - deterministic 32-bit linear congruential generator for the
 * random simulator. Not part of the library. */

typedef struct prng
{
    unsigned long state;
} prng;

void prng_seed(prng *p, unsigned long seed);
unsigned long prng_next(prng *p);
unsigned long prng_below(prng *p, unsigned long bound);

#endif /* PRNG_H */
