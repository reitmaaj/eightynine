/* raft89_replication.c - AppendEntries handling on the follower:
 * consistency check, conflict repair, commit advancement, and reply. */
#include <limits.h>
#include <stddef.h>

#include "raft89_internal.h"

static int send_ae_failure(raft89 *node, raft89_id leader, raft89__u64 term)
{
    raft89_message msg;
    int rc;
    raft89__build_ae_response(
        node->self, leader, raft89__to_public(term), 0, RAFT89_INDEX_NONE,
        RAFT89_TERM_NONE,
        raft89__to_public(raft89__u64_inc(node->last_log_index)), &msg);
    rc = raft89__emit_send(node, &msg);
    return rc;
}

static int ae_stash(raft89 *node, const raft89_message *msg)
{
    const raft89_append_entries *ae;
    int rc;
    ae = &msg->u.append_entries;
    node->ae_prev_index = raft89__from_public(ae->prev_log_index);
    node->ae_prev_term = raft89__from_public(ae->prev_log_term);
    node->ae_leader_commit = raft89__from_public(ae->leader_commit);
    node->ae_leader_id = msg->from;
    node->ae_match_index = raft89__from_public(ae->prev_log_index);
    node->ae_truncate_first = (raft89__u64)0;
    node->ae_conflict_term = (raft89__u64)0;
    node->ae_conflict_index = (raft89__u64)0;
    node->ae_append_offset = 0u;
    node->ae_reply_success = 0;
    rc = raft89__entries_stash(node, ae->entries, ae->entry_count);
    return rc;
}

static int ae_prefix_matches(raft89 *node)
{
    int rc;
    raft89_term term;
    if (raft89__u64_is_zero(node->ae_prev_index))
    {
        return 1;
    }
    if (node->ae_prev_index > node->last_log_index)
    {
        return 0;
    }
    rc = node->store.log_term(node->store.ctx,
                              raft89__to_public(node->ae_prev_index), &term);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return -1;
    }
    if (raft89__from_public(term) == node->ae_prev_term)
    {
        return 1;
    }
    return 0;
}

static void mark_empty_log(raft89 *node)
{
    node->last_log_index = (raft89__u64)0;
    node->last_log_term = (raft89__u64)0;
}

static int ae_update_last(raft89 *node, raft89__u64 index)
{
    int rc;
    raft89_term term;
    if (raft89__u64_is_zero(index))
    {
        mark_empty_log(node);
        return RAFT89_OK;
    }
    rc = node->store.log_term(node->store.ctx, raft89__to_public(index), &term);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return RAFT89_ERR_STORE;
    }
    node->last_log_index = index;
    node->last_log_term = raft89__from_public(term);
    return RAFT89_OK;
}

int raft89__ae_send_reply(raft89 *node)
{
    raft89_message msg;
    int rc;
    node->step = RAFT89_STEP_NONE;
    raft89__build_ae_response(
        node->self, node->ae_leader_id, raft89__to_public(node->current_term),
        node->ae_reply_success, raft89__to_public(node->ae_match_index),
        raft89__to_public(node->ae_conflict_term),
        raft89__to_public(node->ae_conflict_index), &msg);
    rc = raft89__emit_send(node, &msg);
    return rc;
}

static int ae_finish(raft89 *node)
{
    raft89__u64 new_commit;
    int rc;
    new_commit = node->ae_leader_commit;
    new_commit = raft89__min_index(new_commit, node->last_log_index);
    new_commit = raft89__min_index(new_commit, node->ae_match_index);
    raft89__entries_clear(node);
    rc = raft89__advance_commit(node, new_commit, RAFT89_STEP_AE_REPLY);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    if (node->step == RAFT89_STEP_APPLY)
    {
        return RAFT89_OK;
    }
    rc = raft89__ae_send_reply(node);
    return rc;
}

static int emit_log_truncate(raft89 *node, raft89__u64 first)
{
    int rc;
    rc = raft89__action_begin(node, RAFT89_ACT_LOG_TRUNCATE);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    node->action.u.log_truncate.first_index = raft89__to_public(first);
    return RAFT89_OK;
}

static int ae_emit_append(raft89 *node)
{
    int rc;
    raft89_size count;
    count = node->pending_count - node->ae_append_offset;
    if (count == 0u)
    {
        rc = ae_finish(node);
        return rc;
    }
    rc = raft89__action_begin(node, RAFT89_ACT_LOG_APPEND);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    node->action.u.log_append.entries =
        node->pending_entries + node->ae_append_offset;
    node->action.u.log_append.entry_count = count;
    node->step = RAFT89_STEP_AE_APPEND;
    return RAFT89_OK;
}

static void mark_missing_suffix(raft89 *node, raft89_size i)
{
    node->ae_truncate_first = (raft89__u64)0;
    node->ae_append_offset = i;
}

static int ae_scan_one(raft89 *node, raft89_size i)
{
    int rc;
    raft89__u64 index;
    raft89_term term;
    index = node->ae_prev_index + (raft89__u64)1 + raft89__size_to_u64(i);
    if (index > node->last_log_index)
    {
        mark_missing_suffix(node, i);
        return 1;
    }
    rc = node->store.log_term(node->store.ctx, raft89__to_public(index), &term);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return -1;
    }
    if (raft89__from_public(term) ==
        raft89__from_public(node->pending_entries[i].term))
    {
        return 0;
    }
    node->ae_truncate_first = index;
    node->ae_append_offset = i;
    return 1;
}

static int ae_scan(raft89 *node)
{
    raft89_size i;
    int result;
    for (i = 0u; i < node->pending_count; ++i)
    {
        result = ae_scan_one(node, i);
        if (result != 0)
        {
            return result;
        }
    }
    return 0;
}

static int ae_apply_entries(raft89 *node)
{
    int rc;
    int scan;
    scan = ae_scan(node);
    if (scan < 0)
    {
        return RAFT89_ERR_STORE;
    }
    if (scan == 0)
    {
        rc = ae_finish(node);
        return rc;
    }
    if (raft89__u64_is_zero(node->ae_truncate_first))
    {
        rc = ae_emit_append(node);
        return rc;
    }
    if (node->ae_truncate_first <= node->commit_index)
    {
        return RAFT89_ERR_PROTOCOL;
    }
    node->step = RAFT89_STEP_AE_TRUNCATE;
    rc = emit_log_truncate(node, node->ae_truncate_first);
    return rc;
}

/* One backward step of the conflicting-term search. Returns 1 when the
 * run start has been found, 0 to continue, or -1 on a store failure. */
static int conflict_walk(raft89 *node, raft89__u64 *first)
{
    raft89_term term;
    int rc;
    rc = node->store.log_term(
        node->store.ctx, raft89__to_public(raft89__u64_dec(*first)), &term);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return -1;
    }
    if (raft89__from_public(term) != node->ae_conflict_term)
    {
        return 1;
    }
    *first = raft89__u64_dec(*first);
    return 0;
}

/* One loop iteration of the conflicting-term search. Returns 0 to
 * continue, 1 when the run start is known, or -1 on a store failure. */
static int conflict_search(raft89 *node, raft89__u64 *first, int *step)
{
    if (*step != 0)
    {
        return 1;
    }
    *step = conflict_walk(node, first);
    if (*step < 0)
    {
        return -1;
    }
    return 0;
}

/* A follower log shorter than prev_log_index points the leader at the
 * follower's end. */
static int conflict_short_log(raft89 *node)
{
    node->ae_conflict_term = (raft89__u64)0;
    node->ae_conflict_index = raft89__u64_inc(node->last_log_index);
    return RAFT89_OK;
}

/* Compute conflict acceleration hints for a rejected prefix. */
static int conflict_hints(raft89 *node)
{
    raft89_term term;
    raft89__u64 first;
    int step;
    int result;
    int rc;
    if (node->ae_prev_index > node->last_log_index)
    {
        rc = conflict_short_log(node);
        return rc;
    }
    rc = node->store.log_term(node->store.ctx,
                              raft89__to_public(node->ae_prev_index), &term);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return RAFT89_ERR_STORE;
    }
    node->ae_conflict_term = raft89__from_public(term);
    first = node->ae_prev_index;
    step = 0;
    while (first > (raft89__u64)1)
    {
        result = conflict_search(node, &first, &step);
        if (result != 0)
        {
            break;
        }
    }
    if (step < 0)
    {
        return RAFT89_ERR_STORE;
    }
    node->ae_conflict_index = first;
    return RAFT89_OK;
}

static int reply_prefix_failure(raft89 *node)
{
    int rc;
    node->ae_reply_success = 0;
    node->ae_match_index = (raft89__u64)0;
    rc = conflict_hints(node);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = raft89__ae_send_reply(node);
    return rc;
}

static int ae_process(raft89 *node)
{
    int rc;
    int matched;
    node->role = RAFT89_FOLLOWER;
    node->leader_id = node->ae_leader_id;
    node->election_elapsed = 0u;
    node->election_timeout = raft89__pick_timeout(node);
    matched = ae_prefix_matches(node);
    if (matched < 0)
    {
        return RAFT89_ERR_STORE;
    }
    if (matched == 0)
    {
        rc = reply_prefix_failure(node);
        return rc;
    }
    node->ae_reply_success = 1;
    node->ae_match_index =
        node->ae_prev_index + raft89__size_to_u64(node->pending_count);
    rc = ae_apply_entries(node);
    return rc;
}

int raft89__ae_continue(raft89 *node)
{
    int rc;
    rc = ae_process(node);
    return rc;
}

int raft89__ae_after_truncate(raft89 *node)
{
    int rc;
    rc = ae_update_last(node, raft89__u64_dec(node->ae_truncate_first));
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = ae_emit_append(node);
    return rc;
}

int raft89__ae_after_append(raft89 *node)
{
    int rc;
    rc = ae_update_last(node, node->ae_prev_index +
                                  raft89__size_to_u64(node->pending_count));
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = ae_finish(node);
    return rc;
}

static int start_ae_hard_state(raft89 *node, raft89__u64 term)
{
    int rc;
    node->role = RAFT89_FOLLOWER;
    node->leader_id = RAFT89_ID_NONE;
    node->step = RAFT89_STEP_AE_HARD_STATE;
    rc = raft89__emit_hard_state(node, term, RAFT89_ID_NONE);
    return rc;
}

int raft89__recv_append_entries(raft89 *node, const raft89_message *msg)
{
    const raft89_append_entries *ae;
    raft89__u64 term;
    int rc;
    ae = &msg->u.append_entries;
    term = raft89__from_public(ae->term);
    if (term < node->current_term)
    {
        rc = send_ae_failure(node, msg->from, node->current_term);
        return rc;
    }
    if (term == node->current_term)
    {
        if (node->role == RAFT89_LEADER)
        {
            return RAFT89_ERR_PROTOCOL;
        }
    }
    rc = ae_stash(node, msg);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    if (term > node->current_term)
    {
        rc = start_ae_hard_state(node, term);
        return rc;
    }
    rc = ae_process(node);
    return rc;
}
