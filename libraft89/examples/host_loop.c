/* host_loop.c - reference host loop for libraft89.
 *
 * Demonstrates the four boundaries (messages in, commands in, store
 * reads, actions out) with an in-memory store, a loopback packet queue,
 * and the drain loop. The peer responses are synthesized so the example
 * stays a single process. */
#include <stdio.h>
#include <string.h>

#include "raft89.h"

#define MEMBER_COUNT 2
#define QUEUE_MAX 8
#define ENTRY_MAX 16
#define ENTRY_BYTES 64

typedef struct mem_entry
{
    raft89_term term;
    raft89_index index;
    unsigned long size;
    unsigned char data[ENTRY_BYTES];
} mem_entry;

typedef struct mem_store
{
    raft89_hard_state hard;
    mem_entry entries[ENTRY_MAX];
    unsigned long entry_count;
} mem_store;

typedef struct mem_packet
{
    raft89_id from;
    raft89_id to;
    raft89_message msg;
} mem_packet;

typedef struct loopback
{
    mem_packet items[QUEUE_MAX];
    unsigned long count;
} loopback;

static unsigned long host_lo(raft89_u64 v)
{
    return (unsigned long)v.lo;
}

static raft89_u64 host_u64(unsigned long v)
{
    return raft89_u64_from_u32((raft89_u32)v);
}

static int store_hard_state(void *ctx, raft89_hard_state *state)
{
    mem_store *store;
    store = (mem_store *)ctx;
    *state = store->hard;
    return RAFT89_OK;
}

static int store_log_last(void *ctx, raft89_index *index, raft89_term *term)
{
    mem_store *store;
    mem_entry *last;
    store = (mem_store *)ctx;
    if (store->entry_count == 0u)
    {
        *index = RAFT89_INDEX_NONE;
        *term = RAFT89_TERM_NONE;
        return RAFT89_OK;
    }
    last = &store->entries[store->entry_count - 1u];
    *index = last->index;
    *term = last->term;
    return RAFT89_OK;
}

static mem_entry *store_find(mem_store *store, raft89_index index)
{
    unsigned long i;
    i = host_lo(index);
    if (i == 0ul || i > store->entry_count)
    {
        return NULL;
    }
    return &store->entries[i - 1u];
}

static int store_log_term(void *ctx, raft89_index index, raft89_term *term)
{
    mem_store *store;
    mem_entry *entry;
    store = (mem_store *)ctx;
    entry = store_find(store, index);
    if (entry == NULL)
    {
        return RAFT89_ERR_STORE;
    }
    *term = entry->term;
    return RAFT89_OK;
}

static int store_log_size(void *ctx, raft89_index index, raft89_size *size)
{
    mem_store *store;
    mem_entry *entry;
    store = (mem_store *)ctx;
    entry = store_find(store, index);
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
    mem_store *store;
    mem_entry *entry;
    store = (mem_store *)ctx;
    entry = store_find(store, index);
    if (entry == NULL || entry->size != size)
    {
        return RAFT89_ERR_STORE;
    }
    if (size != 0u)
    {
        memcpy(data, entry->data, size);
    }
    return RAFT89_OK;
}

static void store_bind(raft89_store *api, mem_store *store)
{
    api->ctx = store;
    api->hard_state = store_hard_state;
    api->log_last = store_log_last;
    api->log_term = store_log_term;
    api->log_size = store_log_size;
    api->log_read = store_log_read;
}

static unsigned long random_zero(void *ctx, unsigned long upper_exclusive)
{
    (void)ctx;
    (void)upper_exclusive;
    return 0u;
}

static void packet_enqueue(loopback *net, raft89_id from, raft89_id to,
                           const raft89_message *msg)
{
    if (net->count >= (unsigned long)QUEUE_MAX)
    {
        return;
    }
    net->items[net->count].from = from;
    net->items[net->count].to = to;
    net->items[net->count].msg = *msg;
    ++net->count;
}

static void apply_command(const raft89_entry *entry)
{
    printf("applied index %lu payload %.*s\n", host_lo(entry->index),
           (int)entry->size, (const char *)entry->data);
}

static int run_action(raft89 *node, const raft89_action *action,
                      mem_store *store, loopback *net)
{
    unsigned long i;
    if (action->type == RAFT89_ACT_SEND)
    {
        packet_enqueue(net, action->u.send.message.from,
                       action->u.send.message.to, &action->u.send.message);
    }
    if (action->type == RAFT89_ACT_HARD_STATE)
    {
        store->hard = action->u.hard_state.state;
    }
    if (action->type == RAFT89_ACT_LOG_APPEND)
    {
        for (i = 0u; i < action->u.log_append.entry_count; ++i)
        {
            const raft89_entry *entry;
            mem_entry *slot;
            entry = &action->u.log_append.entries[i];
            if (host_lo(entry->index) > (unsigned long)ENTRY_MAX)
            {
                return RAFT89_ERR;
            }
            if (entry->size > (raft89_size)ENTRY_BYTES)
            {
                return RAFT89_ERR;
            }
            slot = &store->entries[host_lo(entry->index) - 1u];
            slot->term = entry->term;
            slot->index = entry->index;
            slot->size = entry->size;
            memcpy(slot->data, entry->data, entry->size);
            if (host_lo(entry->index) > store->entry_count)
            {
                store->entry_count = host_lo(entry->index);
            }
        }
    }
    if (action->type == RAFT89_ACT_LOG_TRUNCATE)
    {
        if (host_lo(action->u.log_truncate.first_index) <= store->entry_count)
        {
            store->entry_count =
                host_lo(action->u.log_truncate.first_index) - 1u;
        }
    }
    if (action->type == RAFT89_ACT_APPLY)
    {
        apply_command(&action->u.apply.entry);
    }
    return raft89_action_done(node, action->id, RAFT89_ACTION_OK);
}

static int drain(raft89 *node, mem_store *store, loopback *net)
{
    const raft89_action *action;
    for (;;)
    {
        if (raft89_next_action(node, &action) != RAFT89_OK)
        {
            return RAFT89_OK;
        }
        if (run_action(node, action, store, net) != RAFT89_OK)
        {
            return RAFT89_ERR;
        }
    }
}

static int expect_leader(raft89 *node)
{
    raft89_status status;
    if (raft89_status_get(node, &status) != RAFT89_OK)
    {
        return -1;
    }
    if (status.role != RAFT89_LEADER)
    {
        fprintf(stderr, "host_loop: expected leader, role=%d\n",
                (int)status.role);
        return -1;
    }
    return 0;
}

int main(void)
{
    raft89_id members[MEMBER_COUNT];
    mem_store store;
    loopback net;
    raft89_config config;
    raft89 *node;
    raft89_message msg;
    raft89_index index;
    int rc;

    memset(&store, 0, sizeof(store));
    memset(&net, 0, sizeof(net));
    members[0] = 1u;
    members[1] = 2u;
    memset(&config, 0, sizeof(config));
    config.self = 1u;
    config.members = members;
    config.member_count = MEMBER_COUNT;
    config.heartbeat_interval = 10u;
    config.election_timeout_min = 20u;
    config.election_timeout_max = 30u;
    config.max_append_entries = 8u;
    config.max_append_bytes = 1024u;
    store_bind(&config.store, &store);
    config.random.ctx = NULL;
    config.random.next = random_zero;

    node = NULL;
    rc = raft89_create(&config, &node);
    if (rc != RAFT89_OK)
    {
        fprintf(stderr, "host_loop: create failed %d\n", rc);
        return 1;
    }

    /* Time out, persist the vote, and drain the vote request sends. */
    if (raft89_tick(node, 20u) != RAFT89_OK || drain(node, &store, &net) != 0)
    {
        fprintf(stderr, "host_loop: election start failed\n");
        return 1;
    }

    /* Deliver the peer's granted vote. */
    msg.type = RAFT89_MSG_REQUEST_VOTE_RESPONSE;
    msg.from = 2u;
    msg.to = 1u;
    msg.u.request_vote_response.term = host_u64(1u);
    msg.u.request_vote_response.vote_granted = 1;
    if (raft89_recv(node, &msg) != RAFT89_OK || drain(node, &store, &net) != 0)
    {
        fprintf(stderr, "host_loop: vote response failed\n");
        return 1;
    }
    if (expect_leader(node) != 0)
    {
        return 1;
    }

    /* Propose a command and replicate it. */
    index = raft89_u64_zero();
    rc = raft89_propose(node, "hello", 5u, &index);
    if (rc != RAFT89_OK || drain(node, &store, &net) != 0)
    {
        fprintf(stderr, "host_loop: propose failed %d\n", rc);
        return 1;
    }

    /* Deliver the peer's success response, then apply the commit. */
    msg.type = RAFT89_MSG_APPEND_ENTRIES_RESPONSE;
    msg.from = 2u;
    msg.to = 1u;
    msg.u.append_entries_response.term = host_u64(1u);
    msg.u.append_entries_response.success = 1;
    msg.u.append_entries_response.match_index = index;
    if (raft89_recv(node, &msg) != RAFT89_OK || drain(node, &store, &net) != 0)
    {
        fprintf(stderr, "host_loop: append response failed\n");
        return 1;
    }

    printf("host_loop: queued sends %lu, durable entries %lu\n", net.count,
           store.entry_count);
    raft89_destroy(node);
    return 0;
}
