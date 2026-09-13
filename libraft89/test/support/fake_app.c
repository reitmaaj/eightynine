/* fake_app.c - durable application state with deduplication. */
#include "fake_app.h"

void fake_app_init(fake_app *app)
{
    unsigned long i;
    app->value = 0u;
    app->last_applied = RAFT89_INDEX_NONE;
    app->applied_count = 0u;
    app->replay_count = 0u;
    for (i = 0u; i < FAKE_APP_SLOTS; ++i)
    {
        app->rec[i].index = RAFT89_INDEX_NONE;
        app->rec[i].term = RAFT89_TERM_NONE;
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
    hash = fake_app_hash(entry->data, entry->size);
    rec = &app->rec[entry->index % FAKE_APP_SLOTS];
    if (entry->index > app->last_applied)
    {
        if (entry->index != app->last_applied + 1u)
        {
            return -1;
        }
        app->value = app->value + entry->index;
        rec->index = entry->index;
        rec->term = entry->term;
        rec->hash = hash;
        rec->present = 1;
        app->last_applied = entry->index;
        ++app->applied_count;
        return 0;
    }
    if (rec->present == 0)
    {
        return -1;
    }
    if (rec->index != entry->index)
    {
        return -1;
    }
    if (rec->term != entry->term)
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
