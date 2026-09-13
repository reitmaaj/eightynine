/* raft89_inspect.c - test-only introspection, canonical snapshots, and
 * deep copies of the private raft89 representation. */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "raft89_inspect.h"
#include "raft89_internal.h"

typedef struct writer
{
    unsigned char *buf;
    unsigned long cap;
    unsigned long len;
    int overflow;
} writer;

static void w_u8(writer *w, unsigned long value)
{
    if (w->len >= w->cap)
    {
        w->overflow = 1;
        return;
    }
    w->buf[w->len] = (unsigned char)(value & 0xFFu);
    ++w->len;
}

static void w_ulong(writer *w, unsigned long value)
{
    unsigned char bytes[sizeof(unsigned long)];
    unsigned long width;
    unsigned long tmp;
    unsigned long i;
    width = (unsigned long)sizeof(unsigned long);
    tmp = value;
    for (i = 0u; i < width; ++i)
    {
        bytes[i] = (unsigned char)(tmp & 0xFFu);
        tmp = tmp >> 8;
    }
    for (i = width; i > 0u; --i)
    {
        w_u8(w, (unsigned long)bytes[i - 1u]);
    }
}

static void w_bytes(writer *w, const void *data, unsigned long size)
{
    const unsigned char *bytes;
    unsigned long i;
    if (size == 0u)
    {
        return;
    }
    if (data == NULL)
    {
        w->overflow = 1;
        return;
    }
    bytes = (const unsigned char *)data;
    for (i = 0u; i < size; ++i)
    {
        w_u8(w, (unsigned long)bytes[i]);
    }
}

static void snap_entries(writer *w, const raft89_entry *entries,
                         raft89_size count)
{
    raft89_size i;
    if (entries == NULL)
    {
        w_u8(w, 0u);
        w_ulong(w, 0u);
        return;
    }
    w_u8(w, 1u);
    w_ulong(w, (unsigned long)count);
    for (i = 0u; i < count; ++i)
    {
        w_ulong(w, entries[i].term);
        w_ulong(w, entries[i].index);
        w_ulong(w, entries[i].size);
        w_bytes(w, entries[i].data, entries[i].size);
    }
}

static void snap_message(writer *w, const raft89_message *msg)
{
    w_ulong(w, (unsigned long)msg->type);
    w_ulong(w, msg->from);
    w_ulong(w, msg->to);
    if (msg->type == RAFT89_MSG_REQUEST_VOTE)
    {
        w_ulong(w, msg->u.request_vote.term);
        w_ulong(w, msg->u.request_vote.last_log_index);
        w_ulong(w, msg->u.request_vote.last_log_term);
    }
    if (msg->type == RAFT89_MSG_REQUEST_VOTE_RESPONSE)
    {
        w_ulong(w, msg->u.request_vote_response.term);
        w_ulong(w, (unsigned long)msg->u.request_vote_response.vote_granted);
    }
    if (msg->type == RAFT89_MSG_APPEND_ENTRIES)
    {
        w_ulong(w, msg->u.append_entries.term);
        w_ulong(w, msg->u.append_entries.prev_log_index);
        w_ulong(w, msg->u.append_entries.prev_log_term);
        w_ulong(w, msg->u.append_entries.leader_commit);
        snap_entries(w, msg->u.append_entries.entries,
                     msg->u.append_entries.entry_count);
    }
    if (msg->type == RAFT89_MSG_APPEND_ENTRIES_RESPONSE)
    {
        w_ulong(w, msg->u.append_entries_response.term);
        w_ulong(w, (unsigned long)msg->u.append_entries_response.success);
        w_ulong(w, msg->u.append_entries_response.match_index);
    }
}

static void snap_action(writer *w, const raft89_action *action)
{
    w_ulong(w, (unsigned long)action->type);
    if (action->type == RAFT89_ACT_SEND)
    {
        snap_message(w, &action->u.send.message);
    }
    if (action->type == RAFT89_ACT_HARD_STATE)
    {
        w_ulong(w, action->u.hard_state.state.current_term);
        w_ulong(w, action->u.hard_state.state.voted_for);
    }
    if (action->type == RAFT89_ACT_LOG_APPEND)
    {
        snap_entries(w, action->u.log_append.entries,
                     action->u.log_append.entry_count);
    }
    if (action->type == RAFT89_ACT_LOG_TRUNCATE)
    {
        w_ulong(w, action->u.log_truncate.first_index);
    }
    if (action->type == RAFT89_ACT_APPLY)
    {
        w_ulong(w, action->u.apply.entry.term);
        w_ulong(w, action->u.apply.entry.index);
        w_ulong(w, action->u.apply.entry.size);
        w_bytes(w, action->u.apply.entry.data, action->u.apply.entry.size);
    }
}

static void snap_node(writer *w, const raft89 *node)
{
    raft89_size i;
    w_ulong(w, node->self);
    w_ulong(w, (unsigned long)node->role);
    w_ulong(w, node->current_term);
    w_ulong(w, node->voted_for);
    w_ulong(w, node->leader_id);
    w_ulong(w, node->last_log_index);
    w_ulong(w, node->last_log_term);
    w_ulong(w, node->commit_index);
    w_ulong(w, node->applied_index);
    w_ulong(w, (unsigned long)node->member_count);
    for (i = 0u; i < node->member_count; ++i)
    {
        w_ulong(w, node->members[i]);
    }
    w_ulong(w, (unsigned long)node->peer_count);
    for (i = 0u; i < node->peer_count; ++i)
    {
        w_ulong(w, node->peers[i]);
    }
    w_ulong(w, node->votes_granted);
    for (i = 0u; i < node->member_count; ++i)
    {
        w_u8(w, (unsigned long)node->votes[i]);
    }
    for (i = 0u; i < node->member_count; ++i)
    {
        w_ulong(w, node->next_index[i]);
    }
    for (i = 0u; i < node->member_count; ++i)
    {
        w_ulong(w, node->match_index[i]);
    }
    w_ulong(w, node->heartbeat_interval);
    w_ulong(w, node->election_timeout_min);
    w_ulong(w, node->election_timeout_max);
    w_ulong(w, node->election_timeout);
    w_ulong(w, node->election_elapsed);
    w_ulong(w, node->heartbeat_elapsed);
    w_ulong(w, node->max_append_entries);
    w_ulong(w, node->max_append_bytes);
    w_u8(w, (unsigned long)node->faulted);
    w_u8(w, (unsigned long)node->has_action);
    if (node->has_action != 0)
    {
        snap_action(w, &node->action);
    }
    w_ulong(w, (unsigned long)node->step);
    w_ulong(w, node->peer_cursor);
    snap_message(w, &node->pending_msg);
    snap_entries(w, node->pending_entries, node->pending_count);
    w_ulong(w, node->ae_prev_index);
    w_ulong(w, node->ae_prev_term);
    w_ulong(w, node->ae_leader_commit);
    w_ulong(w, node->ae_leader_id);
    w_ulong(w, node->ae_match_index);
    w_ulong(w, node->ae_truncate_first);
    w_ulong(w, node->ae_append_offset);
    w_ulong(w, (unsigned long)node->ae_reply_success);
    w_ulong(w, node->apply_size);
    w_bytes(w, node->apply_buffer, node->apply_size);
    w_ulong(w, (unsigned long)node->after_apply);
    snap_entries(w, node->out_entries, node->out_count);
}

void raft89_inspect_view_get(const raft89 *node, raft89_inspect_view *view)
{
    view->role = node->role;
    view->current_term = node->current_term;
    view->voted_for = node->voted_for;
    view->leader_id = node->leader_id;
    view->last_log_index = node->last_log_index;
    view->last_log_term = node->last_log_term;
    view->commit_index = node->commit_index;
    view->applied_index = node->applied_index;
    view->election_timeout = node->election_timeout;
    view->election_elapsed = node->election_elapsed;
    view->heartbeat_elapsed = node->heartbeat_elapsed;
    view->step = (unsigned long)node->step;
    view->votes_granted = node->votes_granted;
    view->member_count = node->member_count;
    view->peer_count = node->peer_count;
    view->has_action = node->has_action;
    view->faulted = node->faulted;
}

int raft89_inspect_peer(const raft89 *node, raft89_size index, raft89_id *id,
                        raft89_index *next_index, raft89_index *match_index)
{
    raft89_size member;
    if (index >= node->peer_count)
    {
        return -1;
    }
    member = raft89__member_index(node, node->peers[index]);
    *id = node->peers[index];
    *next_index = node->next_index[member];
    *match_index = node->match_index[member];
    return 0;
}

int raft89_inspect_vote(const raft89 *node, raft89_size index, int *granted)
{
    if (index >= node->member_count)
    {
        return -1;
    }
    *granted = (int)node->votes[index];
    return 0;
}

const raft89_action *raft89_inspect_action(const raft89 *node)
{
    if (node->has_action == 0)
    {
        return NULL;
    }
    return &node->action;
}

unsigned long raft89_inspect_action_seq(const raft89 *node)
{
    return node->action_seq;
}

static int copy_ids(raft89_id **dst, const raft89_id *src, raft89_size count)
{
    *dst = NULL;
    if (src == NULL || count == 0u)
    {
        return 0;
    }
    *dst = (raft89_id *)malloc(count * sizeof(raft89_id));
    if (*dst == NULL)
    {
        return -1;
    }
    memcpy(*dst, src, count * sizeof(raft89_id));
    return 0;
}

static int copy_indexes(raft89_index **dst, const raft89_index *src,
                        raft89_size count)
{
    *dst = NULL;
    if (src == NULL || count == 0u)
    {
        return 0;
    }
    *dst = (raft89_index *)malloc(count * sizeof(raft89_index));
    if (*dst == NULL)
    {
        return -1;
    }
    memcpy(*dst, src, count * sizeof(raft89_index));
    return 0;
}

static int copy_votes(unsigned char **dst, const unsigned char *src,
                      raft89_size count)
{
    *dst = NULL;
    if (src == NULL || count == 0u)
    {
        return 0;
    }
    *dst = (unsigned char *)malloc((unsigned long)count);
    if (*dst == NULL)
    {
        return -1;
    }
    memcpy(*dst, src, (unsigned long)count);
    return 0;
}

static int entry_block_total(const raft89_entry *src, raft89_size count,
                             unsigned long *total)
{
    unsigned long sum;
    raft89_size i;
    sum = 0u;
    for (i = 0u; i < count; ++i)
    {
        if (src[i].size > ULONG_MAX - sum)
        {
            return -1;
        }
        sum += src[i].size;
    }
    *total = sum;
    return 0;
}

static int copy_entry_block(raft89_entry **dst, raft89_size *dst_count,
                            const raft89_entry *src, raft89_size count)
{
    unsigned long total;
    unsigned long desc;
    unsigned long offset;
    unsigned char *base;
    raft89_entry *entries;
    raft89_size i;
    *dst = NULL;
    *dst_count = 0u;
    if (src == NULL || count == 0u)
    {
        return 0;
    }
    if (entry_block_total(src, count, &total) != 0)
    {
        return -1;
    }
    if (count > ULONG_MAX / (unsigned long)sizeof(raft89_entry))
    {
        return -1;
    }
    desc = (unsigned long)count * (unsigned long)sizeof(raft89_entry);
    if (total > ULONG_MAX - desc)
    {
        return -1;
    }
    base = (unsigned char *)malloc(desc + total);
    if (base == NULL)
    {
        return -1;
    }
    entries = (raft89_entry *)base;
    offset = desc;
    for (i = 0u; i < count; ++i)
    {
        entries[i].term = src[i].term;
        entries[i].index = src[i].index;
        entries[i].size = src[i].size;
        if (src[i].size == 0u)
        {
            entries[i].data = NULL;
        }
        else
        {
            memcpy(base + offset, src[i].data, src[i].size);
            entries[i].data = base + offset;
            offset += src[i].size;
        }
    }
    *dst = entries;
    *dst_count = count;
    return 0;
}

static int clone_arrays(raft89 *dst, const raft89 *src)
{
    if (copy_ids(&dst->members, src->members, src->member_count) != 0)
    {
        return -1;
    }
    if (copy_ids(&dst->peers, src->peers, src->peer_count) != 0)
    {
        return -1;
    }
    if (copy_votes(&dst->votes, src->votes, src->member_count) != 0)
    {
        return -1;
    }
    if (copy_indexes(&dst->next_index, src->next_index, src->member_count) != 0)
    {
        return -1;
    }
    if (copy_indexes(&dst->match_index, src->match_index, src->member_count) !=
        0)
    {
        return -1;
    }
    if (copy_entry_block(&dst->pending_entries, &dst->pending_count,
                         src->pending_entries, src->pending_count) != 0)
    {
        return -1;
    }
    if (copy_entry_block(&dst->out_entries, &dst->out_count, src->out_entries,
                         src->out_count) != 0)
    {
        return -1;
    }
    if (src->apply_buffer != NULL && src->apply_size != 0u)
    {
        dst->apply_buffer = malloc(src->apply_size);
        if (dst->apply_buffer == NULL)
        {
            return -1;
        }
        memcpy(dst->apply_buffer, src->apply_buffer, src->apply_size);
        dst->apply_size = src->apply_size;
    }
    return 0;
}

static unsigned long block_offset(const void *ptr, const void *base)
{
    return (unsigned long)((const unsigned char *)ptr -
                           (const unsigned char *)base);
}

static void fix_entry_pointer(const raft89_entry **dst,
                              const raft89_entry *dst_base,
                              const raft89_entry *src_ptr,
                              const raft89_entry *src_base)
{
    *dst = (const raft89_entry *)((unsigned char *)dst_base +
                                  block_offset(src_ptr, src_base));
}

static void fix_action_pointers(raft89 *dst, const raft89 *src)
{
    if (src->action.type == RAFT89_ACT_LOG_APPEND)
    {
        if (src->action.u.log_append.entries != NULL &&
            src->pending_entries != NULL)
        {
            fix_entry_pointer(
                &dst->action.u.log_append.entries, dst->pending_entries,
                src->action.u.log_append.entries, src->pending_entries);
        }
    }
    if (src->action.type == RAFT89_ACT_SEND)
    {
        if (src->action.u.send.message.type == RAFT89_MSG_APPEND_ENTRIES &&
            src->action.u.send.message.u.append_entries.entries != NULL &&
            src->out_entries != NULL)
        {
            fix_entry_pointer(
                &dst->action.u.send.message.u.append_entries.entries,
                dst->out_entries,
                src->action.u.send.message.u.append_entries.entries,
                src->out_entries);
        }
    }
    if (src->action.type == RAFT89_ACT_APPLY)
    {
        dst->action.u.apply.entry.data = dst->apply_buffer;
    }
    if (src->pending_msg.type == RAFT89_MSG_APPEND_ENTRIES &&
        src->pending_msg.u.append_entries.entries != NULL &&
        src->pending_entries != NULL)
    {
        fix_entry_pointer(
            &dst->pending_msg.u.append_entries.entries, dst->pending_entries,
            src->pending_msg.u.append_entries.entries, src->pending_entries);
    }
}

int raft89_inspect_clone(const raft89 *src, raft89 **out)
{
    raft89 *dst;
    if (src == NULL || out == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    *out = NULL;
    dst = (raft89 *)malloc(sizeof(raft89));
    if (dst == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    *dst = *src;
    dst->members = NULL;
    dst->peers = NULL;
    dst->votes = NULL;
    dst->next_index = NULL;
    dst->match_index = NULL;
    dst->pending_entries = NULL;
    dst->pending_count = 0u;
    dst->apply_buffer = NULL;
    dst->apply_size = 0u;
    dst->out_entries = NULL;
    dst->out_count = 0u;
    if (clone_arrays(dst, src) != 0)
    {
        raft89__node_discard(dst);
        return RAFT89_ERR_NOMEM;
    }
    fix_action_pointers(dst, src);
    *out = dst;
    return RAFT89_OK;
}

void raft89_inspect_rebind(raft89 *node, const raft89_store *store,
                           const raft89_random *random)
{
    node->store = *store;
    node->random = *random;
}

int raft89_inspect_snapshot(const raft89 *node, unsigned char *buf,
                            unsigned long cap, unsigned long *len)
{
    writer w;
    if (node == NULL || buf == NULL || len == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    w.buf = buf;
    w.cap = cap;
    w.len = 0u;
    w.overflow = 0;
    snap_node(&w, node);
    *len = w.len;
    if (w.overflow != 0)
    {
        return RAFT89_ERR_LIMIT;
    }
    return RAFT89_OK;
}

int raft89_inspect_message_snapshot(const raft89_message *msg,
                                    unsigned char *buf, unsigned long cap,
                                    unsigned long *len)
{
    writer w;
    if (msg == NULL || buf == NULL || len == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    w.buf = buf;
    w.cap = cap;
    w.len = 0u;
    w.overflow = 0;
    snap_message(&w, msg);
    *len = w.len;
    if (w.overflow != 0)
    {
        return RAFT89_ERR_LIMIT;
    }
    return RAFT89_OK;
}
