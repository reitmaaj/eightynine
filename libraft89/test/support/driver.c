/* driver.c - test helpers for driving the action protocol. */
#include "driver.h"

static raft89_u64 driver_u64(unsigned long v)
{
    return raft89_u64_from_u32((raft89_u32)v);
}

static unsigned long driver_ul(raft89_u64 v)
{
    return (unsigned long)v.lo;
}

int driver_ack_next(raft89 *node, enum raft89_action_result result)
{
    const raft89_action *action;
    int rc;
    rc = raft89_next_action(node, &action);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    return raft89_action_done(node, action->id, result);
}

int driver_take_send(raft89 *node, raft89_message *out)
{
    const raft89_action *action;
    int rc;
    rc = raft89_next_action(node, &action);
    if (rc != RAFT89_OK)
    {
        return 0;
    }
    if (action->type != RAFT89_ACT_SEND)
    {
        return 0;
    }
    *out = action->u.send.message;
    rc = raft89_action_done(node, action->id, RAFT89_ACTION_OK);
    if (rc != RAFT89_OK)
    {
        return -1;
    }
    return 1;
}

int driver_drain(raft89 *node)
{
    const raft89_action *action;
    int count;
    count = 0;
    for (;;)
    {
        if (raft89_next_action(node, &action) != RAFT89_OK)
        {
            return count;
        }
        if (raft89_action_done(node, action->id, RAFT89_ACTION_OK) != RAFT89_OK)
        {
            return -1;
        }
        ++count;
    }
}

int driver_expect_hard_state(raft89 *node, unsigned long term,
                             raft89_id voted_for)
{
    const raft89_action *action;
    int rc;
    rc = raft89_next_action(node, &action);
    if (rc != RAFT89_OK)
    {
        return 0;
    }
    if (action->type != RAFT89_ACT_HARD_STATE)
    {
        return 0;
    }
    if (!raft89_u64_equal(action->u.hard_state.state.current_term,
                          driver_u64(term)))
    {
        return 0;
    }
    if (action->u.hard_state.state.voted_for != voted_for)
    {
        return 0;
    }
    rc = raft89_action_done(node, action->id, RAFT89_ACTION_OK);
    if (rc != RAFT89_OK)
    {
        return 0;
    }
    return 1;
}

unsigned long driver_collect_sends(raft89 *node, raft89_id *to,
                                   unsigned long max)
{
    raft89_message msg;
    unsigned long count;
    int rc;
    count = 0u;
    for (;;)
    {
        rc = driver_take_send(node, &msg);
        if (rc != 1)
        {
            return count;
        }
        if (count < max)
        {
            to[count] = msg.to;
        }
        ++count;
    }
}

void driver_build_vote_request(raft89_id from, raft89_id to, unsigned long term,
                               unsigned long last_index,
                               unsigned long last_term, raft89_message *msg)
{
    msg->type = RAFT89_MSG_REQUEST_VOTE;
    msg->from = from;
    msg->to = to;
    msg->u.request_vote.term = driver_u64(term);
    msg->u.request_vote.last_log_index = driver_u64(last_index);
    msg->u.request_vote.last_log_term = driver_u64(last_term);
}

void driver_build_vote_response(raft89_id from, raft89_id to,
                                unsigned long term, int granted,
                                raft89_message *msg)
{
    msg->type = RAFT89_MSG_REQUEST_VOTE_RESPONSE;
    msg->from = from;
    msg->to = to;
    msg->u.request_vote_response.term = driver_u64(term);
    msg->u.request_vote_response.vote_granted = granted;
}

void driver_build_ae(raft89_id from, raft89_id to, unsigned long term,
                     unsigned long prev_index, unsigned long prev_term,
                     unsigned long leader_commit, const raft89_entry *entries,
                     raft89_size entry_count, raft89_message *msg)
{
    msg->type = RAFT89_MSG_APPEND_ENTRIES;
    msg->from = from;
    msg->to = to;
    msg->u.append_entries.term = driver_u64(term);
    msg->u.append_entries.prev_log_index = driver_u64(prev_index);
    msg->u.append_entries.prev_log_term = driver_u64(prev_term);
    msg->u.append_entries.leader_commit = driver_u64(leader_commit);
    msg->u.append_entries.entries = entries;
    msg->u.append_entries.entry_count = entry_count;
}

void driver_store_effect(fake_store *store, const raft89_action *action)
{
    unsigned long i;
    const raft89_entry *entry;
    if (action->type == RAFT89_ACT_LOG_APPEND)
    {
        for (i = 0u; i < action->u.log_append.entry_count; ++i)
        {
            entry = &action->u.log_append.entries[i];
            fake_store_put(store, driver_ul(entry->term),
                           driver_ul(entry->index), entry->data, entry->size);
        }
        return;
    }
    if (action->type == RAFT89_ACT_LOG_TRUNCATE)
    {
        fake_store_truncate(store,
                            driver_ul(action->u.log_truncate.first_index));
    }
}

int driver_ack_with_effect(fake_store *store, raft89 *node)
{
    const raft89_action *action;
    int rc;
    rc = raft89_next_action(node, &action);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    driver_store_effect(store, action);
    return raft89_action_done(node, action->id, RAFT89_ACTION_OK);
}
