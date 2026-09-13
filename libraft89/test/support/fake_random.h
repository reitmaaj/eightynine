#ifndef FAKE_RANDOM_H
#define FAKE_RANDOM_H

/* fake_random.h - deterministic scripted random source for tests.
 * Values are consumed in order and wrap around. Not part of the
 * library. */

#include <raft89.h>

#define FAKE_RANDOM_MAX 16

typedef struct fake_random
{
    unsigned long values[FAKE_RANDOM_MAX];
    unsigned long count;
    unsigned long pos;
} fake_random;

void fake_random_init(fake_random *r);
void fake_random_push(fake_random *r, unsigned long value);
void fake_random_bind(raft89_random *api, fake_random *r);

#endif /* FAKE_RANDOM_H */
