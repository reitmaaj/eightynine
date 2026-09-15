/* fixture.c - shared test fixture. */
#include "fixture.h"

void fixture_init(fixture *f, raft89_size member_count, raft89_id self)
{
    raft89_size i;
    fake_store_init(&f->store);
    fake_random_init(&f->random);
    for (i = 0u; i < member_count; ++i)
    {
        f->members[i] = i + 1u;
    }
    f->config.self = self;
    f->config.members = f->members;
    f->config.member_count = member_count;
    f->config.heartbeat_interval = 10u;
    f->config.election_timeout_min = 20u;
    f->config.election_timeout_max = 30u;
    f->config.max_append_entries = 8u;
    f->config.max_append_bytes = 1024u;
    f->config.applied_index = raft89_u64_zero();
    fake_store_bind(&f->config.store, &f->store);
    fake_random_bind(&f->config.random, &f->random);
}

void fixture_rebind(fixture *f)
{
    f->config.members = f->members;
    fake_store_bind(&f->config.store, &f->store);
    fake_random_bind(&f->config.random, &f->random);
}
