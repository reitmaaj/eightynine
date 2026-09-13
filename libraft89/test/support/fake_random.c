/* fake_random.c - deterministic scripted random source. */
#include "fake_random.h"

static unsigned long fake_random_next(void *ctx, unsigned long upper_exclusive)
{
    fake_random *r;
    unsigned long value;
    r = (fake_random *)ctx;
    if (r->count == 0u)
    {
        return 0u;
    }
    value = r->values[r->pos];
    r->pos = (r->pos + 1u) % r->count;
    return value % upper_exclusive;
}

void fake_random_init(fake_random *r)
{
    unsigned long i;
    for (i = 0u; i < FAKE_RANDOM_MAX; ++i)
    {
        r->values[i] = 0u;
    }
    r->count = 0u;
    r->pos = 0u;
}

void fake_random_push(fake_random *r, unsigned long value)
{
    if (r->count >= FAKE_RANDOM_MAX)
    {
        return;
    }
    r->values[r->count] = value;
    ++r->count;
}

void fake_random_bind(raft89_random *api, fake_random *r)
{
    api->ctx = r;
    api->next = fake_random_next;
}
