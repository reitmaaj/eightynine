/* raft89_node.c - lifecycle, restore, status, and action queries. */
#include <stdlib.h>
#include <string.h>

#include "raft89_internal.h"

static raft89_id *members_copy(const raft89_id *src, raft89_size count)
{
    raft89_id *dst;
    dst = (raft89_id *)malloc(count * sizeof(raft89_id));
    if (dst == NULL)
    {
        return NULL;
    }
    memcpy(dst, src, count * sizeof(raft89_id));
    return dst;
}

static void peers_add(raft89_id *peers, raft89_size *count, raft89_id id)
{
    peers[*count] = id;
    ++(*count);
}

static raft89_id *peers_copy(const raft89_config *config)
{
    raft89_id *peers;
    raft89_size i;
    raft89_size count;
    peers = (raft89_id *)malloc(config->member_count * sizeof(raft89_id));
    if (peers == NULL)
    {
        return NULL;
    }
    count = 0u;
    for (i = 0u; i < config->member_count; ++i)
    {
        if (config->members[i] != config->self)
        {
            peers_add(peers, &count, config->members[i]);
        }
    }
    return peers;
}

static int state_alloc(raft89 *node, const raft89_config *config)
{
    node->members = members_copy(config->members, config->member_count);
    if (node->members == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    node->peers = peers_copy(config);
    if (node->peers == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    node->votes =
        (unsigned char *)calloc(config->member_count, sizeof(unsigned char));
    if (node->votes == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    node->next_index =
        (raft89__u64 *)calloc(config->member_count, sizeof(raft89__u64));
    if (node->next_index == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    node->match_index =
        (raft89__u64 *)calloc(config->member_count, sizeof(raft89__u64));
    if (node->match_index == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    node->member_count = config->member_count;
    node->peer_count = config->member_count - 1u;
    return RAFT89_OK;
}

void raft89__node_init(raft89 *node, const raft89_config *config)
{
    node->self = config->self;
    node->role = RAFT89_FOLLOWER;
    node->current_term = (raft89__u64)0;
    node->voted_for = RAFT89_ID_NONE;
    node->leader_id = RAFT89_ID_NONE;
    node->last_log_index = (raft89__u64)0;
    node->last_log_term = (raft89__u64)0;
    node->commit_index = (raft89__u64)0;
    node->applied_index = (raft89__u64)0;
    node->votes_granted = 0u;
    node->heartbeat_interval = config->heartbeat_interval;
    node->election_timeout_min = config->election_timeout_min;
    node->election_timeout_max = config->election_timeout_max;
    node->election_timeout = config->election_timeout_min;
    node->election_elapsed = 0u;
    node->heartbeat_elapsed = 0u;
    node->max_append_entries = config->max_append_entries;
    node->max_append_bytes = config->max_append_bytes;
    node->store = config->store;
    node->random = config->random;
    node->faulted = 0;
    node->action_seq = RAFT89_ACTION_ID_NONE;
    node->has_action = 0;
    memset(&node->action, 0, sizeof(node->action));
    node->step = RAFT89_STEP_NONE;
    node->peer_cursor = 0u;
    memset(&node->pending_msg, 0, sizeof(node->pending_msg));
    node->pending_entries = NULL;
    node->pending_count = 0u;
    node->ae_prev_index = (raft89__u64)0;
    node->ae_prev_term = (raft89__u64)0;
    node->ae_leader_commit = (raft89__u64)0;
    node->ae_leader_id = RAFT89_ID_NONE;
    node->ae_match_index = (raft89__u64)0;
    node->ae_truncate_first = (raft89__u64)0;
    node->ae_append_offset = 0u;
    node->ae_reply_success = 0;
    node->apply_buffer = NULL;
    node->apply_size = 0u;
    node->after_apply = RAFT89_STEP_NONE;
    node->out_entries = NULL;
    node->out_count = 0u;
}

void raft89__node_discard(raft89 *node)
{
    if (node == NULL)
    {
        return;
    }
    free(node->members);
    free(node->peers);
    free(node->votes);
    free(node->next_index);
    free(node->match_index);
    free(node->pending_entries);
    free(node->apply_buffer);
    free(node->out_entries);
    free(node);
}

int raft89__store_read_hard_state(raft89 *node)
{
    int rc;
    raft89_hard_state state;
    rc = node->store.hard_state(node->store.ctx, &state);
    if (rc != RAFT89_OK)
    {
        return RAFT89_ERR_STORE;
    }
    node->current_term = raft89__from_public(state.current_term);
    node->voted_for = state.voted_for;
    return RAFT89_OK;
}

int raft89__store_read_log_last(raft89 *node)
{
    int rc;
    raft89_index index;
    raft89_term term;
    rc = node->store.log_last(node->store.ctx, &index, &term);
    if (rc != RAFT89_OK)
    {
        return RAFT89_ERR_STORE;
    }
    node->last_log_index = raft89__from_public(index);
    node->last_log_term = raft89__from_public(term);
    return RAFT89_OK;
}

int raft89__node_has_member(const raft89 *node, raft89_id id)
{
    raft89_size i;
    for (i = 0u; i < node->member_count; ++i)
    {
        if (node->members[i] == id)
        {
            return 1;
        }
    }
    return 0;
}

int raft89__check_restored(const raft89 *node)
{
    if (raft89__u64_is_zero(node->last_log_index))
    {
        if (!raft89__u64_is_zero(node->last_log_term))
        {
            return RAFT89_ERR_CORRUPT;
        }
    }
    if (!raft89__u64_is_zero(node->last_log_index))
    {
        if (raft89__u64_is_zero(node->last_log_term))
        {
            return RAFT89_ERR_CORRUPT;
        }
    }
    if (node->voted_for != RAFT89_ID_NONE)
    {
        if (raft89__node_has_member(node, node->voted_for) == 0)
        {
            return RAFT89_ERR_CORRUPT;
        }
    }
    return RAFT89_OK;
}

int raft89__node_load(raft89 *node)
{
    int rc;
    rc = raft89__store_read_hard_state(node);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = raft89__store_read_log_last(node);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    return raft89__check_restored(node);
}

static raft89 *node_alloc(const raft89_config *config)
{
    raft89 *node;
    int rc;
    node = (raft89 *)malloc(sizeof(raft89));
    if (node == NULL)
    {
        return NULL;
    }
    node->members = NULL;
    node->peers = NULL;
    node->votes = NULL;
    node->next_index = NULL;
    node->match_index = NULL;
    node->pending_entries = NULL;
    node->apply_buffer = NULL;
    node->out_entries = NULL;
    node->member_count = 0u;
    node->peer_count = 0u;
    rc = state_alloc(node, config);
    if (rc != RAFT89_OK)
    {
        raft89__node_discard(node);
        return NULL;
    }
    raft89__node_init(node, config);
    return node;
}

int raft89_create(const raft89_config *config, raft89 **out)
{
    int rc;
    raft89 *node;
    if (out == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    *out = NULL;
    rc = raft89__config_validate(config);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    node = node_alloc(config);
    if (node == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    rc = raft89__node_load(node);
    if (rc != RAFT89_OK)
    {
        raft89__node_discard(node);
        return rc;
    }
    node->election_timeout = raft89__pick_timeout(node);
    *out = node;
    return RAFT89_OK;
}

void raft89_destroy(raft89 *node)
{
    raft89__node_discard(node);
}

int raft89_status_get(const raft89 *node, raft89_status *status)
{
    if (node == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (status == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    status->self = node->self;
    status->role = node->role;
    status->current_term = raft89__to_public(node->current_term);
    status->voted_for = node->voted_for;
    status->leader_id = node->leader_id;
    status->last_log_index = raft89__to_public(node->last_log_index);
    status->commit_index = raft89__to_public(node->commit_index);
    status->applied_index = raft89__to_public(node->applied_index);
    status->faulted = node->faulted;
    return RAFT89_OK;
}

int raft89_next_action(raft89 *node, const raft89_action **action)
{
    if (node == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (action == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (node->faulted != 0)
    {
        return RAFT89_ERR_FAULTED;
    }
    if (node->has_action == 0)
    {
        *action = NULL;
        return RAFT89_EMPTY;
    }
    *action = &node->action;
    return RAFT89_OK;
}
