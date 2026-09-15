/* fake_app.c - durable application state with deduplication. */
#include "fake_app.h"

static unsigned long app_u64_lo(raft89_u64 v)
{
    return (unsigned long)v.lo;
}

void fake_app_init(fake_app *app)
{
    unsigned long i;
    app->value = 0u;
    app->last_applied = 0ul;
    app->applied_count = 0u;
    app->replay_count = 0u;
    for (i = 0u; i < FAKE_APP_SLOTS; ++i)
    {
        app->rec[i].index = 0ul;
        app->rec[i].term = 0ul;
        app->rec[i].hash = 0u;
        app->rec[i].present = 0;
    }
}

unsigned long fake_app_hash(const void *data, unsigned long size)
{
    const unsigned char *bytes;
    unsigned long hash;
    unsigned long i;
    bytes = (const unsigned char *)data;
    hash = 5381u;
    for (i = 0u; i < size; ++i)
    {
        hash = hash * 33u + bytes[i];
    }
    return hash;
}

int fake_app_apply(fake_app *app, const raft89_entry *entry)
{
    fake_app_rec *rec;
    unsigned long hash;
    unsigned long index;
    unsigned long term;
    hash = fake_app_hash(entry->data, entry->size);
    index = app_u64_lo(entry->index);
    term = app_u64_lo(entry->term);
    rec = &app->rec[index % FAKE_APP_SLOTS];
    if (index > app->last_applied)
    {
        if (index != app->last_applied + 1u)
        {
            return -1;
        }
        app->value = app->value + index;
        rec->index = index;
        rec->term = term;
        rec->hash = hash;
        rec->present = 1;
        app->last_applied = index;
        ++app->applied_count;
        return 0;
    }
    if (rec->present == 0)
    {
        return -1;
    }
    if (rec->index != index)
    {
        return -1;
    }
    if (rec->term != term)
    {
        return -1;
    }
    if (rec->hash != hash)
    {
        return -1;
    }
    ++app->replay_count;
    return 0;
}
