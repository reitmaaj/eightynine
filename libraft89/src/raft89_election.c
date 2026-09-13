/* raft89_election.c - elections, vote requests, vote counting, and
 * leadership acquisition. */
#include <limits.h>

#include "raft89_internal.h"

raft89_size raft89__quorum(const raft89 *node)
{
    return (node->member_count / 2u) + 1u;
}

raft89_size raft89__member_index(const raft89 *node, raft89_id id)
{
    raft89_size i;
    for (i = 0u; i < node->member_count; ++i)
    {
        if (node->members[i] == id)
        {
            return i;
        }
    }
    return node->member_count;
}

int raft89__log_is_fresh(raft89_term cand_term, raft89_index cand_index,
                         raft89_term our_term, raft89_index our_index)
{
    if (cand_term != our_term)
    {
        return cand_term > our_term;
    }
    return cand_index >= our_index;
}

static void reset_votes(raft89 *node)
{
    raft89_size i;
    for (i = 0u; i < node->member_count; ++i)
    {
        node->votes[i] = 0u;
    }
    node->votes[raft89__member_index(node, node->self)] = 1u;
}

static int reply_vote(raft89 *node, raft89_id peer, raft89_term term,
                      int granted)
{
    raft89_message msg;
    int rc;
    raft89__build_vote_response(node->self, peer, term, granted, &msg);
    rc = raft89__emit_send(node, &msg);
    return rc;
}

static int persist_vote_then_reply(raft89 *node, raft89_id peer,
                                   raft89_term term, raft89_id vote,
                                   int granted)
{
    int rc;
    raft89__build_vote_response(node->self, peer, term, granted,
                                &node->pending_msg);
    rc = raft89__emit_hard_state(node, term, vote);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    node->step = RAFT89_STEP_VOTE_RESPONSE;
    return RAFT89_OK;
}

int raft89__step_down_new_term(raft89 *node, raft89_term term)
{
    int rc;
    node->role = RAFT89_FOLLOWER;
    node->leader_id = RAFT89_ID_NONE;
    node->election_elapsed = 0u;
    node->election_timeout = raft89__pick_timeout(node);
    node->step = RAFT89_STEP_STEP_DOWN;
    rc = raft89__emit_hard_state(node, term, RAFT89_ID_NONE);
    return rc;
}

int raft89__start_election(raft89 *node)
{
    int rc;
    raft89_term term;
    if (node->current_term == ULONG_MAX)
    {
        node->faulted = 1;
        return RAFT89_ERR_LIMIT;
    }
    term = node->current_term + 1u;
    node->role = RAFT89_CANDIDATE;
    node->leader_id = RAFT89_ID_NONE;
    node->election_elapsed = 0u;
    node->election_timeout = raft89__pick_timeout(node);
    reset_votes(node);
    node->votes_granted = 1u;
    rc = raft89__emit_hard_state(node, term, node->self);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    node->peer_cursor = 0u;
    node->step = RAFT89_STEP_ELECTION_BROADCAST;
    return RAFT89_OK;
}

static int become_leader_after_quorum(raft89 *node)
{
    int rc;
    node->step = RAFT89_STEP_NONE;
    rc = raft89__become_leader(node);
    return rc;
}

int raft89__election_send_next(raft89 *node)
{
    raft89_id peer;
    raft89_message msg;
    int rc;
    if (node->votes_granted >= raft89__quorum(node))
    {
        rc = become_leader_after_quorum(node);
        return rc;
    }
    if (node->peer_cursor >= node->peer_count)
    {
        node->step = RAFT89_STEP_NONE;
        return RAFT89_OK;
    }
    peer = node->peers[node->peer_cursor];
    ++node->peer_cursor;
    raft89__build_request_vote(node, peer, &msg);
    node->step = RAFT89_STEP_ELECTION_BROADCAST;
    rc = raft89__emit_send(node, &msg);
    return rc;
}

static void init_peer_state(raft89 *node, raft89_size index)
{
    node->next_index[index] = node->last_log_index + 1u;
    node->match_index[index] = RAFT89_INDEX_NONE;
}

int raft89__become_leader(raft89 *node)
{
    raft89_size i;
    int rc;
    if (node->last_log_index == ULONG_MAX)
    {
        node->faulted = 1;
        return RAFT89_ERR_LIMIT;
    }
    node->role = RAFT89_LEADER;
    node->leader_id = node->self;
    node->heartbeat_elapsed = 0u;
    node->election_elapsed = 0u;
    for (i = 0u; i < node->member_count; ++i)
    {
        init_peer_state(node, i);
    }
    node->peer_cursor = 0u;
    node->step = RAFT89_STEP_LEADER_BROADCAST;
    rc = raft89__leader_broadcast_next(node);
    return rc;
}

static raft89_id fresh_vote(const raft89_message *msg, int fresh)
{
    if (fresh != 0)
    {
        return msg->from;
    }
    return RAFT89_ID_NONE;
}

static int vote_new_term(raft89 *node, const raft89_message *msg, int fresh)
{
    raft89_term term;
    raft89_id vote;
    int rc;
    term = msg->u.request_vote.term;
    vote = fresh_vote(msg, fresh);
    node->role = RAFT89_FOLLOWER;
    node->leader_id = RAFT89_ID_NONE;
    node->election_elapsed = 0u;
    node->election_timeout = raft89__pick_timeout(node);
    rc = persist_vote_then_reply(node, msg->from, term, vote, fresh);
    return rc;
}

int raft89__recv_request_vote(raft89 *node, const raft89_message *msg)
{
    raft89_term term;
    int fresh;
    int rc;
    term = msg->u.request_vote.term;
    if (term < node->current_term)
    {
        rc = reply_vote(node, msg->from, node->current_term, 0);
        return rc;
    }
    fresh = raft89__log_is_fresh(msg->u.request_vote.last_log_term,
                                 msg->u.request_vote.last_log_index,
                                 node->last_log_term, node->last_log_index);
    if (term > node->current_term)
    {
        rc = vote_new_term(node, msg, fresh);
        return rc;
    }
    if (node->voted_for == msg->from)
    {
        rc = reply_vote(node, msg->from, term, 1);
        return rc;
    }
    if (node->voted_for != RAFT89_ID_NONE)
    {
        rc = reply_vote(node, msg->from, term, 0);
        return rc;
    }
    if (node->role == RAFT89_LEADER)
    {
        rc = reply_vote(node, msg->from, term, 0);
        return rc;
    }
    if (fresh == 0)
    {
        rc = reply_vote(node, msg->from, term, 0);
        return rc;
    }
    rc = persist_vote_then_reply(node, msg->from, term, msg->from, 1);
    return rc;
}

int raft89__recv_vote_response(raft89 *node, const raft89_message *msg)
{
    raft89_term term;
    raft89_size index;
    int rc;
    term = msg->u.request_vote_response.term;
    if (term < node->current_term)
    {
        return RAFT89_OK;
    }
    if (term > node->current_term)
    {
        rc = raft89__step_down_new_term(node, term);
        return rc;
    }
    if (node->role != RAFT89_CANDIDATE)
    {
        return RAFT89_OK;
    }
    if (msg->u.request_vote_response.vote_granted == 0)
    {
        return RAFT89_OK;
    }
    index = raft89__member_index(node, msg->from);
    if (node->votes[index] != 0u)
    {
        return RAFT89_OK;
    }
    node->votes[index] = 1u;
    ++node->votes_granted;
    if (node->votes_granted < raft89__quorum(node))
    {
        return RAFT89_OK;
    }
    rc = raft89__become_leader(node);
    return rc;
}
