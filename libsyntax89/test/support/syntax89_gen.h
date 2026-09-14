#ifndef SYNTAX89_GEN_H
#define SYNTAX89_GEN_H

/* Deterministic pseudorandom generator for model and generated suites. */
struct syntax89_rng
{
    unsigned long state;
};

void syntax89_rng_seed(struct syntax89_rng *r, unsigned long seed);

/* Next value (xorshift64). */
unsigned long syntax89_rng_next(struct syntax89_rng *r);

/* Uniform value in [0, n) for n > 0. */
unsigned long syntax89_rng_below(struct syntax89_rng *r, unsigned long n);

#endif
