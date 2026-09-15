/* fake_store.c - in-memory durable store with fault injection. */
#include <string.h>

#include "fake_store.h"

static raft89_u64 store_u64(unsigned long v)
{
    return raft89_u64_from_u32((raft89_u32)v);
}

static unsigned long store_lo(raft89_u64 v)
{
    return (unsigned long)v.lo;
}

static fake_store_entry *find_entry(fake_store *store, unsigned long index)
{
    if (index == 0ul)
    {
        return NULL;
    }
    if (index > store->entry_count)
    {
        return NULL;
    }
    return &store->entries[index - 1ul];
}

static int store_hard_state(void *ctx, raft89_hard_state *state)
{
    fake_store *store;
    store = (fake_store *)ctx;
    if (store->fail_hard_state != 0)
    {
        return RAFT89_ERR_STORE;
    }
    *state = store->hard;
    return RAFT89_OK;
}

static int store_log_last(void *ctx, raft89_index *index, raft89_term *term)
{
    fake_store *store;
    fake_store_entry *last;
    store = (fake_store *)ctx;
    if (store->fail_log_last != 0)
    {
        return RAFT89_ERR_STORE;
    }
    if (store->override_last != 0)
    {
        *index = store->last_index_override;
        *term = store->last_term_override;
        return RAFT89_OK;
    }
    if (store->entry_count == 0u)
    {
        *index = raft89_u64_zero();
        *term = raft89_u64_zero();
        return RAFT89_OK;
    }
    last = &store->entries[store->entry_count - 1u];
    *index = last->index;
    *term = last->term;
    return RAFT89_OK;
}

static int store_log_term(void *ctx, raft89_index index, raft89_term *term)
{
    fake_store *store;
    fake_store_entry *entry;
    store = (fake_store *)ctx;
    if (store->fail_log_term != 0)
    {
        return RAFT89_ERR_STORE;
    }
    entry = find_entry(store, store_lo(index));
    if (entry == NULL)
    {
        return RAFT89_ERR_STORE;
    }
    *term = entry->term;
    return RAFT89_OK;
}

static int store_log_size(void *ctx, raft89_index index, raft89_size *size)
{
    fake_store *store;
    fake_store_entry *entry;
    store = (fake_store *)ctx;
    if (store->fail_log_size != 0)
    {
        return RAFT89_ERR_STORE;
    }
    entry = find_entry(store, store_lo(index));
    if (entry == NULL)
    {
        return RAFT89_ERR_STORE;
    }
    *size = entry->size;
    return RAFT89_OK;
}

static int store_log_read(void *ctx, raft89_index index, void *data,
                          raft89_size size)
{
    fake_store *store;
    fake_store_entry *entry;
    store = (fake_store *)ctx;
    if (store->fail_log_read != 0)
    {
        return RAFT89_ERR_STORE;
    }
    entry = find_entry(store, store_lo(index));
    if (entry == NULL)
    {
        return RAFT89_ERR_STORE;
    }
    if (entry->size != size)
    {
        return RAFT89_ERR_STORE;
    }
    if (size != 0u)
    {
        memcpy(data, entry->data, size);
    }
    return RAFT89_OK;
}

void fake_store_init(fake_store *store)
{
    memset(store, 0, sizeof(*store));
    store->hard.current_term = raft89_u64_zero();
    store->hard.voted_for = RAFT89_ID_NONE;
    store->last_index_override = raft89_u64_zero();
    store->last_term_override = raft89_u64_zero();
}

void fake_store_set_hard(fake_store *store, unsigned long term,
                         raft89_id voted_for)
{
    store->hard.current_term = store_u64(term);
    store->hard.voted_for = voted_for;
}

int fake_store_put(fake_store *store, unsigned long term, unsigned long index,
                   const void *data, unsigned long size)
{
    fake_store_entry *entry;
    if (index == 0ul)
    {
        return RAFT89_ERR_ARG;
    }
    if (index > FAKE_STORE_MAX_ENTRIES)
    {
        return RAFT89_ERR_NOMEM;
    }
    if (size > FAKE_STORE_MAX_PAYLOAD)
    {
        return RAFT89_ERR_NOMEM;
    }
    entry = &store->entries[index - 1ul];
    entry->term = store_u64(term);
    entry->index = store_u64(index);
    entry->size = size;
    if (size != 0u)
    {
        memcpy(entry->data, data, size);
    }
    if (index > store->entry_count)
    {
        store->entry_count = index;
    }
    return RAFT89_OK;
}

void fake_store_truncate(fake_store *store, unsigned long first_index)
{
    if (first_index == 0ul)
    {
        return;
    }
    if (first_index > store->entry_count)
    {
        return;
    }
    store->entry_count = first_index - 1ul;
}

int fake_store_append(fake_store *store, unsigned long term,
                      unsigned long index, const void *data, unsigned long size)
{
    return fake_store_put(store, term, index, data, size);
}

void fake_store_bind(raft89_store *api, fake_store *store)
{
    api->ctx = store;
    api->hard_state = store_hard_state;
    api->log_last = store_log_last;
    api->log_term = store_log_term;
    api->log_size = store_log_size;
    api->log_read = store_log_read;
}
