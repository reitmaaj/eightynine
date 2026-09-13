/* raft89_replication.c - AppendEntries handling on the follower:
 * consistency check, conflict repair, commit advancement, and reply. */
#include <limits.h>
#include <stddef.h>

#include "raft89_internal.h"

static int send_ae_failure(raft89 *node, raft89_id leader, raft89_term term)
{
    raft89_message msg;
    int rc;
    raft89__build_ae_response(node->self, leader, term, 0, RAFT89_INDEX_NONE,
                              &msg);
    rc = raft89__emit_send(node, &msg);
    return rc;
}

static int ae_stash(raft89 *node, const raft89_message *msg)
{
    const raft89_append_entries *ae;
    int rc;
    ae = &msg->u.append_entries;
    node->ae_prev_index = ae->prev_log_index;
    node->ae_prev_term = ae->prev_log_term;
    node->ae_leader_commit = ae->leader_commit;
    node->ae_leader_id = msg->from;
    node->ae_match_index = ae->prev_log_index;
    node->ae_truncate_first = RAFT89_INDEX_NONE;
    node->ae_append_offset = 0u;
    node->ae_reply_success = 0;
    rc = raft89__entries_stash(node, ae->entries, ae->entry_count);
    return rc;
}

static int ae_prefix_matches(raft89 *node)
{
    int rc;
    raft89_term term;
    if (node->ae_prev_index == RAFT89_INDEX_NONE)
    {
        return 1;
    }
    if (node->ae_prev_index > node->last_log_index)
    {
        return 0;
    }
    rc = node->store.log_term(node->store.ctx, node->ae_prev_index, &term);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return -1;
    }
    if (term == node->ae_prev_term)
    {
        return 1;
    }
    return 0;
}

static void mark_empty_log(raft89 *node)
{
    node->last_log_index = RAFT89_INDEX_NONE;
    node->last_log_term = RAFT89_TERM_NONE;
}

static int ae_update_last(raft89 *node, raft89_index index)
{
    int rc;
    raft89_term term;
    if (index == RAFT89_INDEX_NONE)
    {
        mark_empty_log(node);
        return RAFT89_OK;
    }
    rc = node->store.log_term(node->store.ctx, index, &term);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return RAFT89_ERR_STORE;
    }
    node->last_log_index = index;
    node->last_log_term = term;
    return RAFT89_OK;
}

int raft89__ae_send_reply(raft89 *node)
{
    raft89_message msg;
    int rc;
    node->step = RAFT89_STEP_NONE;
    raft89__build_ae_response(node->self, node->ae_leader_id,
                              node->current_term, node->ae_reply_success,
                              node->ae_match_index, &msg);
    rc = raft89__emit_send(node, &msg);
    return rc;
}

static int ae_finish(raft89 *node)
{
    raft89_index new_commit;
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

static int emit_log_truncate(raft89 *node, raft89_index first)
{
    int rc;
    rc = raft89__action_begin(node, RAFT89_ACT_LOG_TRUNCATE);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    node->action.u.log_truncate.first_index = first;
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
    node->ae_truncate_first = RAFT89_INDEX_NONE;
    node->ae_append_offset = i;
}

static int ae_scan_one(raft89 *node, raft89_size i)
{
    int rc;
    raft89_index index;
    raft89_term term;
    index = node->ae_prev_index + 1u + i;
    if (index > node->last_log_index)
    {
        mark_missing_suffix(node, i);
        return 1;
    }
    rc = node->store.log_term(node->store.ctx, index, &term);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return -1;
    }
    if (term == node->pending_entries[i].term)
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
    if (node->ae_truncate_first == RAFT89_INDEX_NONE)
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

static int reply_prefix_failure(raft89 *node)
{
    int rc;
    node->ae_reply_success = 0;
    node->ae_match_index = RAFT89_INDEX_NONE;
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
    node->ae_match_index = node->ae_prev_index + node->pending_count;
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
    rc = ae_update_last(node, node->ae_truncate_first - 1u);
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
    rc = ae_update_last(node, node->ae_prev_index + node->pending_count);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = ae_finish(node);
    return rc;
}

static int start_ae_hard_state(raft89 *node, raft89_term term)
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
    int rc;
    ae = &msg->u.append_entries;
    if (ae->term < node->current_term)
    {
        rc = send_ae_failure(node, msg->from, node->current_term);
        return rc;
    }
    if (ae->term == node->current_term)
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
    if (ae->term > node->current_term)
    {
        rc = start_ae_hard_state(node, ae->term);
        return rc;
    }
    rc = ae_process(node);
    return rc;
}
