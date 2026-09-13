#ifndef FIXTURE_H
#define FIXTURE_H

/* fixture.h - shared test fixture: a valid configuration over an
 * in-memory store and scripted random source. Not part of the library. */

#include <raft89.h>

#include "fake_random.h"
#include "fake_store.h"

typedef struct fixture
{
    fake_store store;
    fake_random random;
    raft89_id members[5];
    raft89_config config;
} fixture;

void fixture_init(fixture *f, raft89_size member_count, raft89_id self);

/* Re-point config at this fixture's own arrays, store, and random
 * source. Required after a fixture is copied by value. */
void fixture_rebind(fixture *f);

#endif /* FIXTURE_H */
