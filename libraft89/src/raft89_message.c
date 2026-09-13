/* raft89_message.c - message validation, construction, and receive
 * dispatch. */
#include <limits.h>
#include <stddef.h>

#include "raft89_internal.h"

int raft89__request_vote_valid(const raft89_request_vote *rv)
{
    if (rv->last_log_index == RAFT89_INDEX_NONE)
    {
        if (rv->last_log_term != RAFT89_TERM_NONE)
        {
            return 0;
        }
    }
    if (rv->last_log_index != RAFT89_INDEX_NONE)
    {
        if (rv->last_log_term == RAFT89_TERM_NONE)
        {
            return 0;
        }
    }
    return 1;
}

int raft89__vote_response_valid(const raft89_request_vote_response *response)
{
    if (response->vote_granted != 0)
    {
        if (response->vote_granted != 1)
        {
            return 0;
        }
    }
    return 1;
}

static int entry_shape_valid(const raft89_append_entries *ae, raft89_size i)
{
    const raft89_entry *entry;
    entry = &ae->entries[i];
    if (entry->index == RAFT89_INDEX_NONE)
    {
        return 0;
    }
    if (entry->term == RAFT89_TERM_NONE)
    {
        return 0;
    }
    if (entry->index != ae->prev_log_index + 1u + i)
    {
        return 0;
    }
    if (entry->size != 0u)
    {
        if (entry->data == NULL)
        {
            return 0;
        }
    }
    return 1;
}

static int entries_shape_valid(const raft89_append_entries *ae)
{
    raft89_size i;
    int valid;
    for (i = 0u; i < ae->entry_count; ++i)
    {
        valid = entry_shape_valid(ae, i);
        if (valid == 0)
        {
            return 0;
        }
    }
    return 1;
}

int raft89__append_entries_valid(const raft89_append_entries *ae,
                                 raft89_size max_entries, raft89_size max_bytes)
{
    unsigned long total;
    raft89_size i;
    if (ae->entry_count > max_entries)
    {
        return 0;
    }
    if (ae->entry_count == 0u)
    {
        return 1;
    }
    if (ae->entries == NULL)
    {
        return 0;
    }
    if (ae->prev_log_index > ULONG_MAX - ae->entry_count)
    {
        return 0;
    }
    total = 0u;
    for (i = 0u; i < ae->entry_count; ++i)
    {
        total = raft89__sat_add(total, ae->entries[i].size);
    }
    if (total > max_bytes)
    {
        return 0;
    }
    return entries_shape_valid(ae);
}

int raft89__append_entries_response_valid(
    const raft89_append_entries_response *response)
{
    if (response->success != 0)
    {
        if (response->success != 1)
        {
            return 0;
        }
    }
    if (response->success == 0)
    {
        if (response->match_index != RAFT89_INDEX_NONE)
        {
            return 0;
        }
    }
    return 1;
}

int raft89__msg_envelope_valid(const raft89 *node, const raft89_message *msg)
{
    int valid;
    if (msg->to != node->self)
    {
        return 0;
    }
    if (msg->from == node->self)
    {
        return 0;
    }
    if (msg->from == RAFT89_ID_NONE)
    {
        return 0;
    }
    if (raft89__node_has_member(node, msg->from) == 0)
    {
        return 0;
    }
    if (msg->type == RAFT89_MSG_REQUEST_VOTE)
    {
        return raft89__request_vote_valid(&msg->u.request_vote);
    }
    if (msg->type == RAFT89_MSG_REQUEST_VOTE_RESPONSE)
    {
        return raft89__vote_response_valid(&msg->u.request_vote_response);
    }
    if (msg->type == RAFT89_MSG_APPEND_ENTRIES)
    {
        valid = raft89__append_entries_valid(&msg->u.append_entries,
                                             node->max_append_entries,
                                             node->max_append_bytes);
        return valid;
    }
    if (msg->type == RAFT89_MSG_APPEND_ENTRIES_RESPONSE)
    {
        valid = raft89__append_entries_response_valid(
            &msg->u.append_entries_response);
        return valid;
    }
    return 0;
}

void raft89__build_request_vote(const raft89 *node, raft89_id peer,
                                raft89_message *msg)
{
    msg->type = RAFT89_MSG_REQUEST_VOTE;
    msg->from = node->self;
    msg->to = peer;
    msg->u.request_vote.term = node->current_term;
    msg->u.request_vote.last_log_index = node->last_log_index;
    msg->u.request_vote.last_log_term = node->last_log_term;
}

void raft89__build_vote_response(raft89_id from, raft89_id peer,
                                 raft89_term term, int granted,
                                 raft89_message *msg)
{
    msg->type = RAFT89_MSG_REQUEST_VOTE_RESPONSE;
    msg->from = from;
    msg->to = peer;
    msg->u.request_vote_response.term = term;
    msg->u.request_vote_response.vote_granted = granted;
}

void raft89__build_ae_response(raft89_id from, raft89_id peer, raft89_term term,
                               int success, raft89_index match_index,
                               raft89_message *msg)
{
    msg->type = RAFT89_MSG_APPEND_ENTRIES_RESPONSE;
    msg->from = from;
    msg->to = peer;
    msg->u.append_entries_response.term = term;
    msg->u.append_entries_response.success = success;
    msg->u.append_entries_response.match_index = match_index;
}

int raft89_recv(raft89 *node, const raft89_message *message)
{
    int valid;
    int rc;
    if (node == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (message == NULL)
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
    valid = raft89__msg_envelope_valid(node, message);
    if (valid == 0)
    {
        return RAFT89_ERR_PROTOCOL;
    }
    if (message->type == RAFT89_MSG_REQUEST_VOTE)
    {
        rc = raft89__recv_request_vote(node, message);
        return rc;
    }
    if (message->type == RAFT89_MSG_REQUEST_VOTE_RESPONSE)
    {
        rc = raft89__recv_vote_response(node, message);
        return rc;
    }
    if (message->type == RAFT89_MSG_APPEND_ENTRIES)
    {
        rc = raft89__recv_append_entries(node, message);
        return rc;
    }
    if (message->type == RAFT89_MSG_APPEND_ENTRIES_RESPONSE)
    {
        rc = raft89__recv_ae_response(node, message);
        return rc;
    }
    return RAFT89_ERR_STATE;
}
