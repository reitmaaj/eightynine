#ifndef FAKE_APP_H
#define FAKE_APP_H

/* fake_app.h - durable application state with index-keyed deduplication
 * for crash tests. Not part of the library. */

#include <raft89.h>

#define FAKE_APP_SLOTS 64

typedef struct fake_app_rec
{
    raft89_index index;
    raft89_term term;
    unsigned long hash;
    int present;
} fake_app_rec;

typedef struct fake_app
{
    unsigned long value;
    raft89_index last_applied;
    fake_app_rec rec[FAKE_APP_SLOTS];
    unsigned long applied_count;
    unsigned long replay_count;
} fake_app;

void fake_app_init(fake_app *app);
unsigned long fake_app_hash(const void *data, unsigned long size);

/* Apply one entry. Fresh and identical-replay applications return 0; a
 * conflicting replay or an out-of-order index returns -1. */
int fake_app_apply(fake_app *app, const raft89_entry *entry);

#endif /* FAKE_APP_H */
