/* raft89_action.c - one-outstanding-action queue and continuations. */
#include <limits.h>
#include <string.h>

#include "raft89_internal.h"

int raft89__action_result_valid(const raft89_action *action,
                                enum raft89_action_result result)
{
    if (result == RAFT89_ACTION_OK)
    {
        return 1;
    }
    if (result == RAFT89_ACTION_FATAL)
    {
        return 1;
    }
    if (result == RAFT89_ACTION_LOST)
    {
        return action->type == RAFT89_ACT_SEND;
    }
    return 0;
}

int raft89__action_begin(raft89 *node, enum raft89_action_type type)
{
    if (node->action_seq == ULONG_MAX)
    {
        node->faulted = 1;
        return RAFT89_ERR_LIMIT;
    }
    node->action_seq = node->action_seq + 1u;
    node->action.id = node->action_seq;
    node->action.type = type;
    memset(&node->action.u, 0, sizeof(node->action.u));
    node->has_action = 1;
    return RAFT89_OK;
}

int raft89__emit_send(raft89 *node, const raft89_message *msg)
{
    int rc;
    rc = raft89__action_begin(node, RAFT89_ACT_SEND);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    node->action.u.send.message = *msg;
    return RAFT89_OK;
}

int raft89__emit_hard_state(raft89 *node, raft89_term term, raft89_id voted_for)
{
    int rc;
    rc = raft89__action_begin(node, RAFT89_ACT_HARD_STATE);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    node->action.u.hard_state.state.current_term = term;
    node->action.u.hard_state.state.voted_for = voted_for;
    return RAFT89_OK;
}

void raft89__apply_hard_state(raft89 *node)
{
    node->current_term = node->action.u.hard_state.state.current_term;
    node->voted_for = node->action.u.hard_state.state.voted_for;
}

static int vote_response_send(raft89 *node)
{
    int rc;
    node->step = RAFT89_STEP_NONE;
    rc = raft89__emit_send(node, &node->pending_msg);
    return rc;
}

int raft89__resume(raft89 *node)
{
    int rc;
    if (node->step == RAFT89_STEP_ELECTION_BROADCAST)
    {
        rc = raft89__election_send_next(node);
        return rc;
    }
    if (node->step == RAFT89_STEP_VOTE_RESPONSE)
    {
        rc = vote_response_send(node);
        return rc;
    }
    if (node->step == RAFT89_STEP_LEADER_BROADCAST)
    {
        rc = raft89__leader_broadcast_next(node);
        return rc;
    }
    if (node->step == RAFT89_STEP_LEADER_APPEND)
    {
        rc = raft89__leader_after_append(node);
        return rc;
    }
    if (node->step == RAFT89_STEP_AE_HARD_STATE)
    {
        rc = raft89__ae_continue(node);
        return rc;
    }
    if (node->step == RAFT89_STEP_AE_TRUNCATE)
    {
        rc = raft89__ae_after_truncate(node);
        return rc;
    }
    if (node->step == RAFT89_STEP_AE_APPEND)
    {
        rc = raft89__ae_after_append(node);
        return rc;
    }
    if (node->step == RAFT89_STEP_AE_REPLY)
    {
        rc = raft89__ae_send_reply(node);
        return rc;
    }
    if (node->step == RAFT89_STEP_APPLY)
    {
        rc = raft89__apply_ack(node);
        return rc;
    }
    node->step = RAFT89_STEP_NONE;
    return RAFT89_OK;
}

int raft89_action_done(raft89 *node, raft89_action_id id,
                       enum raft89_action_result result)
{
    int valid;
    int rc;
    if (node == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (node->faulted != 0)
    {
        return RAFT89_ERR_FAULTED;
    }
    if (node->has_action == 0)
    {
        return RAFT89_ERR_STATE;
    }
    if (id != node->action.id)
    {
        return RAFT89_ERR_STATE;
    }
    valid = raft89__action_result_valid(&node->action, result);
    if (valid == 0)
    {
        return RAFT89_ERR_STATE;
    }
    if (result == RAFT89_ACTION_FATAL)
    {
        node->faulted = 1;
        return RAFT89_OK;
    }
    if (node->action.type == RAFT89_ACT_HARD_STATE)
    {
        raft89__apply_hard_state(node);
    }
    node->has_action = 0;
    rc = raft89__resume(node);
    return rc;
}
