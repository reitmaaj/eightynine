/* raft89_leader.c - leader replication, proposal, and the commit rule. */
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "raft89_internal.h"

static void out_entries_clear(raft89 *node)
{
    free(node->out_entries);
    node->out_entries = NULL;
    node->out_count = 0u;
}

static int out_entry_fill(raft89 *node, raft89_entry *dst, raft89_index index,
                          unsigned char *base, unsigned long *offset)
{
    int rc;
    raft89_term term;
    raft89_size size;
    rc = raft89__log_term_at(node, index, &term);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = node->store.log_size(node->store.ctx, index, &size);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return RAFT89_ERR_STORE;
    }
    dst->term = term;
    dst->index = index;
    dst->size = size;
    dst->data = base + *offset;
    if (size != 0u)
    {
        rc = node->store.log_read(node->store.ctx, index, base + *offset, size);
        if (rc != RAFT89_OK)
        {
            node->faulted = 1;
            return RAFT89_ERR_STORE;
        }
    }
    *offset += size;
    return RAFT89_OK;
}

static int out_entries_build(raft89 *node, raft89_index from,
                             raft89_index count, unsigned long total)
{
    unsigned char *base;
    raft89_entry *entries;
    unsigned long offset;
    raft89_index i;
    int rc;
    out_entries_clear(node);
    if (count == 0u)
    {
        return RAFT89_OK;
    }
    base = raft89__entries_buffer_alloc(count, total);
    if (base == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    entries = (raft89_entry *)base;
    offset = count * sizeof(raft89_entry);
    for (i = 0u; i < count; ++i)
    {
        rc = out_entry_fill(node, &entries[i], from + i, base, &offset);
        if (rc != RAFT89_OK)
        {
            free(base);
            return rc;
        }
    }
    node->out_entries = entries;
    node->out_count = count;
    return RAFT89_OK;
}

static int sum_one(raft89 *node, raft89_index index, unsigned long *sum)
{
    int rc;
    raft89_size size;
    rc = node->store.log_size(node->store.ctx, index, &size);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return RAFT89_ERR_STORE;
    }
    *sum = raft89__sat_add(*sum, size);
    return RAFT89_OK;
}

static int entry_sum(raft89 *node, raft89_index from, raft89_index count,
                     unsigned long *total)
{
    unsigned long sum;
    raft89_index i;
    int rc;
    sum = 0u;
    for (i = 0u; i < count; ++i)
    {
        rc = sum_one(node, from + i, &sum);
        if (rc != RAFT89_OK)
        {
            return rc;
        }
    }
    *total = sum;
    return RAFT89_OK;
}

static int shrink_step(raft89 *node, raft89_index from, raft89_index *count,
                       unsigned long *total)
{
    int rc;
    rc = entry_sum(node, from, *count, total);
    if (rc != RAFT89_OK)
    {
        return -1;
    }
    if (*total <= node->max_append_bytes)
    {
        return 1;
    }
    --(*count);
    return 0;
}

static int shrink_to_fit(raft89 *node, raft89_index from, raft89_index *count,
                         unsigned long *total)
{
    int status;
    status = 0;
    while (status == 0)
    {
        status = shrink_step(node, from, count, total);
        if (status < 0)
        {
            return RAFT89_ERR_STORE;
        }
    }
    return RAFT89_OK;
}

static void mark_empty_batch(raft89_index *count, unsigned long *total)
{
    *count = 0u;
    *total = 0u;
}

static int plan_batch(raft89 *node, raft89_index from, raft89_index *count,
                      unsigned long *total)
{
    raft89_index avail;
    int rc;
    if (from > node->last_log_index)
    {
        mark_empty_batch(count, total);
        return RAFT89_OK;
    }
    avail = node->last_log_index - from + 1u;
    avail = raft89__min_index(avail, node->max_append_entries);
    *count = avail;
    rc = shrink_to_fit(node, from, count, total);
    return rc;
}

static int build_leader_ae(raft89 *node, raft89_id peer, raft89_message *msg)
{
    raft89_size idx;
    raft89_index next;
    raft89_index prev;
    raft89_term prev_term;
    raft89_index count;
    unsigned long total;
    int rc;
    idx = raft89__member_index(node, peer);
    next = node->next_index[idx];
    prev = next - 1u;
    rc = raft89__log_term_at(node, prev, &prev_term);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = plan_batch(node, next, &count, &total);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = out_entries_build(node, next, count, total);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    msg->type = RAFT89_MSG_APPEND_ENTRIES;
    msg->from = node->self;
    msg->to = peer;
    msg->u.append_entries.term = node->current_term;
    msg->u.append_entries.prev_log_index = prev;
    msg->u.append_entries.prev_log_term = prev_term;
    msg->u.append_entries.leader_commit = node->commit_index;
    msg->u.append_entries.entries = node->out_entries;
    msg->u.append_entries.entry_count = count;
    return RAFT89_OK;
}

int raft89__leader_send_to(raft89 *node, raft89_id peer)
{
    raft89_message msg;
    int rc;
    rc = build_leader_ae(node, peer, &msg);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = raft89__emit_send(node, &msg);
    return rc;
}

int raft89__leader_broadcast_next(raft89 *node)
{
    raft89_id peer;
    int rc;
    if (node->peer_cursor >= node->peer_count)
    {
        node->step = RAFT89_STEP_NONE;
        return RAFT89_OK;
    }
    peer = node->peers[node->peer_cursor];
    ++node->peer_cursor;
    node->step = RAFT89_STEP_LEADER_BROADCAST;
    rc = raft89__leader_send_to(node, peer);
    return rc;
}

static int peer_replicated(raft89 *node, raft89_size i, raft89_index index)
{
    if (node->members[i] == node->self)
    {
        return 0;
    }
    if (node->match_index[i] >= index)
    {
        return 1;
    }
    return 0;
}

static unsigned long count_replicated(raft89 *node, raft89_index index)
{
    unsigned long count;
    raft89_size i;
    count = 1u;
    for (i = 0u; i < node->member_count; ++i)
    {
        if (peer_replicated(node, i, index) != 0)
        {
            ++count;
        }
    }
    return count;
}

static raft89_index majority_search(raft89 *node, raft89_index from,
                                    raft89_index stop, unsigned long need)
{
    raft89_index index;
    index = from;
    while (index > stop)
    {
        if (count_replicated(node, index) >= need)
        {
            return index;
        }
        --index;
    }
    return stop;
}

int raft89__leader_maybe_commit(raft89 *node)
{
    raft89_index index;
    raft89_term term;
    int rc;
    index = majority_search(node, node->last_log_index, node->commit_index,
                            raft89__quorum(node));
    if (index <= node->commit_index)
    {
        return RAFT89_OK;
    }
    rc = raft89__log_term_at(node, index, &term);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    if (term != node->current_term)
    {
        return RAFT89_OK;
    }
    rc = raft89__advance_commit(node, index, RAFT89_STEP_NONE);
    return rc;
}

int raft89__leader_after_append(raft89 *node)
{
    int rc;
    raft89_entry *entry;
    entry = node->pending_entries;
    if (entry == NULL)
    {
        node->step = RAFT89_STEP_NONE;
        return RAFT89_OK;
    }
    node->last_log_index = entry->index;
    node->last_log_term = entry->term;
    raft89__entries_clear(node);
    node->peer_cursor = 0u;
    node->step = RAFT89_STEP_LEADER_BROADCAST;
    rc = raft89__leader_broadcast_next(node);
    return rc;
}

static void advance_next(raft89 *node, raft89_size idx, raft89_index match)
{
    node->next_index[idx] = match + 1u;
}

static int handle_ae_failure(raft89 *node, raft89_size idx, raft89_id from)
{
    int rc;
    if (node->next_index[idx] > 1u)
    {
        --node->next_index[idx];
    }
    rc = raft89__leader_send_to(node, from);
    return rc;
}

int raft89__recv_ae_response(raft89 *node, const raft89_message *msg)
{
    raft89_term term;
    raft89_size idx;
    raft89_index match;
    int rc;
    term = msg->u.append_entries_response.term;
    if (term < node->current_term)
    {
        return RAFT89_OK;
    }
    if (term > node->current_term)
    {
        rc = raft89__step_down_new_term(node, term);
        return rc;
    }
    if (node->role != RAFT89_LEADER)
    {
        return RAFT89_OK;
    }
    idx = raft89__member_index(node, msg->from);
    if (msg->u.append_entries_response.success == 0)
    {
        rc = handle_ae_failure(node, idx, msg->from);
        return rc;
    }
    match = msg->u.append_entries_response.match_index;
    if (match <= node->match_index[idx])
    {
        return RAFT89_OK;
    }
    if (match > node->last_log_index)
    {
        return RAFT89_OK;
    }
    node->match_index[idx] = match;
    if (match < ULONG_MAX)
    {
        advance_next(node, idx, match);
    }
    rc = raft89__leader_maybe_commit(node);
    return rc;
}

static void set_index(raft89_index *out, raft89_index value)
{
    *out = value;
}

int raft89_propose(raft89 *node, const void *data, raft89_size size,
                   raft89_index *index)
{
    int rc;
    raft89_entry entry;
    if (node == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (size != 0u)
    {
        if (data == NULL)
        {
            return RAFT89_ERR_ARG;
        }
    }
    if (node->faulted != 0)
    {
        return RAFT89_ERR_FAULTED;
    }
    if (node->has_action != 0)
    {
        return RAFT89_BUSY;
    }
    if (node->role != RAFT89_LEADER)
    {
        return RAFT89_NOT_LEADER;
    }
    if (size > node->max_append_bytes)
    {
        return RAFT89_ERR_LIMIT;
    }
    if (node->last_log_index == ULONG_MAX)
    {
        node->faulted = 1;
        return RAFT89_ERR_LIMIT;
    }
    entry.index = node->last_log_index + 1u;
    entry.term = node->current_term;
    entry.data = data;
    entry.size = size;
    rc = raft89__entries_stash(node, &entry, 1u);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = raft89__action_begin(node, RAFT89_ACT_LOG_APPEND);
    if (rc != RAFT89_OK)
    {
        raft89__entries_clear(node);
        return rc;
    }
    node->action.u.log_append.entries = node->pending_entries;
    node->action.u.log_append.entry_count = 1u;
    node->step = RAFT89_STEP_LEADER_APPEND;
    if (index != NULL)
    {
        set_index(index, entry.index);
    }
    return RAFT89_OK;
}
