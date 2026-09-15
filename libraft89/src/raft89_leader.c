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

static int out_entry_fill(raft89 *node, raft89_entry *dst, raft89__u64 index,
                          unsigned char *base, unsigned long *offset)
{
    int rc;
    raft89__u64 term;
    raft89_size size;
    rc = raft89__log_term_at(node, index, &term);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = node->store.log_size(node->store.ctx, raft89__to_public(index), &size);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return RAFT89_ERR_STORE;
    }
    dst->term = raft89__to_public(term);
    dst->index = raft89__to_public(index);
    dst->size = size;
    dst->data = base + *offset;
    if (size != 0u)
    {
        rc = node->store.log_read(node->store.ctx, raft89__to_public(index),
                                  base + *offset, size);
        if (rc != RAFT89_OK)
        {
            node->faulted = 1;
            return RAFT89_ERR_STORE;
        }
    }
    *offset += size;
    return RAFT89_OK;
}

static int out_entries_build(raft89 *node, raft89__u64 from, raft89__u64 count,
                             unsigned long total)
{
    unsigned char *base;
    raft89_entry *entries;
    raft89_size count_size;
    unsigned long offset;
    raft89__u64 i;
    int ok;
    int rc;
    out_entries_clear(node);
    if (raft89__u64_is_zero(count))
    {
        return RAFT89_OK;
    }
    ok = raft89__u64_to_size(count, &count_size);
    if (!ok)
    {
        return RAFT89_ERR_LIMIT;
    }
    base = raft89__entries_buffer_alloc(count_size, total);
    if (base == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    entries = (raft89_entry *)base;
    offset = count_size * sizeof(raft89_entry);
    for (i = (raft89__u64)0; i < count; i = raft89__u64_inc(i))
    {
        rc = out_entry_fill(node, &entries[i], from + i, base, &offset);
        if (rc != RAFT89_OK)
        {
            free(base);
            return rc;
        }
    }
    node->out_entries = entries;
    node->out_count = count_size;
    return RAFT89_OK;
}

static int sum_one(raft89 *node, raft89__u64 index, unsigned long *sum)
{
    int rc;
    raft89_size size;
    rc = node->store.log_size(node->store.ctx, raft89__to_public(index), &size);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return RAFT89_ERR_STORE;
    }
    *sum = raft89__sat_add(*sum, size);
    return RAFT89_OK;
}

static int entry_sum(raft89 *node, raft89__u64 from, raft89__u64 count,
                     unsigned long *total)
{
    unsigned long sum;
    raft89__u64 i;
    int rc;
    sum = 0u;
    for (i = (raft89__u64)0; i < count; i = raft89__u64_inc(i))
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

static int shrink_step(raft89 *node, raft89__u64 from, raft89__u64 *count,
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
    *count = raft89__u64_dec(*count);
    return 0;
}

static int shrink_to_fit(raft89 *node, raft89__u64 from, raft89__u64 *count,
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

static void mark_empty_batch(raft89__u64 *count, unsigned long *total)
{
    *count = (raft89__u64)0;
    *total = 0u;
}

static int plan_batch(raft89 *node, raft89__u64 from, raft89__u64 *count,
                      unsigned long *total)
{
    raft89__u64 avail;
    int rc;
    if (from > node->last_log_index)
    {
        mark_empty_batch(count, total);
        return RAFT89_OK;
    }
    avail = raft89__u64_inc(node->last_log_index - from);
    avail =
        raft89__min_index(avail, raft89__size_to_u64(node->max_append_entries));
    *count = avail;
    rc = shrink_to_fit(node, from, count, total);
    return rc;
}

static int build_leader_ae(raft89 *node, raft89_id peer, raft89_message *msg)
{
    raft89_size idx;
    raft89__u64 next;
    raft89__u64 prev;
    raft89__u64 prev_term;
    raft89__u64 count;
    raft89_size count_size;
    unsigned long total;
    int ok;
    int rc;
    idx = raft89__member_index(node, peer);
    next = node->next_index[idx];
    prev = raft89__u64_dec(next);
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
    ok = raft89__u64_to_size(count, &count_size);
    if (!ok)
    {
        return RAFT89_ERR_LIMIT;
    }
    msg->type = RAFT89_MSG_APPEND_ENTRIES;
    msg->from = node->self;
    msg->to = peer;
    msg->u.append_entries.term = raft89__to_public(node->current_term);
    msg->u.append_entries.prev_log_index = raft89__to_public(prev);
    msg->u.append_entries.prev_log_term = raft89__to_public(prev_term);
    msg->u.append_entries.leader_commit = raft89__to_public(node->commit_index);
    msg->u.append_entries.entries = node->out_entries;
    msg->u.append_entries.entry_count = count_size;
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

static int broadcast_finish(raft89 *node)
{
    int rc;
    node->step = RAFT89_STEP_NONE;
    rc = raft89__leader_maybe_commit(node);
    return rc;
}

int raft89__leader_broadcast_next(raft89 *node)
{
    raft89_id peer;
    int rc;
    if (node->peer_cursor >= node->peer_count)
    {
        rc = broadcast_finish(node);
        return rc;
    }
    peer = node->peers[node->peer_cursor];
    ++node->peer_cursor;
    node->step = RAFT89_STEP_LEADER_BROADCAST;
    rc = raft89__leader_send_to(node, peer);
    return rc;
}

static int peer_replicated(raft89 *node, raft89_size i, raft89__u64 index)
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

static unsigned long count_replicated(raft89 *node, raft89__u64 index)
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

static raft89__u64 majority_search(raft89 *node, raft89__u64 from,
                                   raft89__u64 stop, unsigned long need)
{
    raft89__u64 index;
    index = from;
    while (index > stop)
    {
        if (count_replicated(node, index) >= need)
        {
            return index;
        }
        index = raft89__u64_dec(index);
    }
    return stop;
}

int raft89__leader_maybe_commit(raft89 *node)
{
    raft89__u64 index;
    raft89__u64 term;
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
    if (node->pending_entries == NULL)
    {
        node->step = RAFT89_STEP_NONE;
        return RAFT89_OK;
    }
    entry = &node->pending_entries[node->pending_count - 1u];
    node->last_log_index = raft89__from_public(entry->index);
    node->last_log_term = raft89__from_public(entry->term);
    raft89__entries_clear(node);
    node->peer_cursor = 0u;
    node->step = RAFT89_STEP_LEADER_BROADCAST;
    rc = raft89__leader_broadcast_next(node);
    return rc;
}

static void advance_next(raft89 *node, raft89_size idx, raft89__u64 match)
{
    node->next_index[idx] = raft89__u64_inc(match);
}

static int handle_ae_failure(raft89 *node, raft89_size idx, raft89_id from)
{
    int rc;
    if (node->next_index[idx] > (raft89__u64)1)
    {
        node->next_index[idx] = raft89__u64_dec(node->next_index[idx]);
    }
    rc = raft89__leader_send_to(node, from);
    return rc;
}

int raft89__recv_ae_response(raft89 *node, const raft89_message *msg)
{
    raft89__u64 term;
    raft89_size idx;
    raft89__u64 match;
    int rc;
    term = raft89__from_public(msg->u.append_entries_response.term);
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
    match = raft89__from_public(msg->u.append_entries_response.match_index);
    if (match <= node->match_index[idx])
    {
        return RAFT89_OK;
    }
    if (match > node->last_log_index)
    {
        return RAFT89_OK;
    }
    node->match_index[idx] = match;
    if (match < RAFT89__U64_MAX)
    {
        advance_next(node, idx, match);
    }
    rc = raft89__leader_maybe_commit(node);
    return rc;
}

static void set_index(raft89_index *out, raft89__u64 value)
{
    *out = raft89__to_public(value);
}

/* True when some command payload pointer is unusable. */
static int commands_bad(const raft89_command *commands, raft89_size count)
{
    raft89_size i;
    for (i = 0u; i < count; ++i)
    {
        if (commands[i].size != 0u)
        {
            if (commands[i].data == NULL)
            {
                return 1;
            }
        }
    }
    return 0;
}

/* Saturating sum of command payload sizes. */
static unsigned long commands_total(const raft89_command *commands,
                                    raft89_size count)
{
    unsigned long sum;
    raft89_size i;
    sum = 0u;
    for (i = 0u; i < count; ++i)
    {
        sum = raft89__sat_add(sum, commands[i].size);
    }
    return sum;
}

static void command_fill(raft89_entry *entry, raft89__u64 index,
                         raft89__u64 term, const raft89_command *command)
{
    entry->index = raft89__to_public(index);
    entry->term = raft89__to_public(term);
    entry->data = command->data;
    entry->size = command->size;
}

int raft89_proposev(raft89 *node, const raft89_command *commands,
                    raft89_size count, raft89_index *first_index)
{
    raft89_entry *entries;
    unsigned long total;
    raft89__u64 first;
    raft89_size i;
    int bad;
    int rc;
    if (node == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (commands == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (count == 0u)
    {
        return RAFT89_ERR_ARG;
    }
    bad = commands_bad(commands, count);
    if (bad)
    {
        return RAFT89_ERR_ARG;
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
    if (count > node->max_append_entries)
    {
        return RAFT89_ERR_LIMIT;
    }
    if (count > ULONG_MAX / sizeof(raft89_entry))
    {
        return RAFT89_ERR_LIMIT;
    }
    total = commands_total(commands, count);
    if (total > node->max_append_bytes)
    {
        return RAFT89_ERR_LIMIT;
    }
    if (node->last_log_index > RAFT89__U64_MAX - raft89__size_to_u64(count))
    {
        node->faulted = 1;
        return RAFT89_ERR_LIMIT;
    }
    entries = (raft89_entry *)malloc(count * sizeof(raft89_entry));
    if (entries == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    first = raft89__u64_inc(node->last_log_index);
    for (i = 0u; i < count; ++i)
    {
        command_fill(&entries[i], first + raft89__size_to_u64(i),
                     node->current_term, &commands[i]);
    }
    rc = raft89__entries_stash(node, entries, count);
    free(entries);
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
    node->action.u.log_append.entry_count = count;
    node->step = RAFT89_STEP_LEADER_APPEND;
    if (first_index != NULL)
    {
        set_index(first_index, first);
    }
    return RAFT89_OK;
}

int raft89_propose(raft89 *node, const void *data, raft89_size size,
                   raft89_index *index)
{
    raft89_command command;
    int rc;
    command.data = data;
    command.size = size;
    rc = raft89_proposev(node, &command, 1u, index);
    return rc;
}
