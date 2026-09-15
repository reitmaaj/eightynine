#ifndef FAKE_STORE_H
#define FAKE_STORE_H

/* fake_store.h - in-memory durable store for tests. Models H and L with
 * per-callback fault injection. Test-facing term/index parameters are
 * unsigned long; the store converts at the public boundary. Not part of
 * the library. */

#include <raft89.h>

#define FAKE_STORE_MAX_ENTRIES 64
#define FAKE_STORE_MAX_PAYLOAD 64

typedef struct fake_store_entry
{
    raft89_term term;
    raft89_index index;
    unsigned long size;
    unsigned char data[FAKE_STORE_MAX_PAYLOAD];
} fake_store_entry;

typedef struct fake_store
{
    raft89_hard_state hard;
    fake_store_entry entries[FAKE_STORE_MAX_ENTRIES];
    unsigned long entry_count;
    int override_last;
    raft89_u64 last_index_override;
    raft89_u64 last_term_override;
    int fail_hard_state;
    int fail_log_last;
    int fail_log_term;
    int fail_log_size;
    int fail_log_read;
} fake_store;

void fake_store_init(fake_store *store);
void fake_store_set_hard(fake_store *store, unsigned long term,
                         raft89_id voted_for);
int fake_store_append(fake_store *store, unsigned long term,
                      unsigned long index, const void *data,
                      unsigned long size);
int fake_store_put(fake_store *store, unsigned long term, unsigned long index,
                   const void *data, unsigned long size);
void fake_store_truncate(fake_store *store, unsigned long first_index);
void fake_store_bind(raft89_store *api, fake_store *store);

#endif /* FAKE_STORE_H */
