#ifndef DRIVER_H
#define DRIVER_H

/* driver.h - test helpers for driving the action protocol. Not part of
 * the library. */

#include <raft89.h>

#include "fake_store.h"

/* Acknowledge the outstanding action with the given result. Returns
 * RAFT89_EMPTY when no action is outstanding. */
int driver_ack_next(raft89 *node, enum raft89_action_result result);

/* If the outstanding action is a SEND, copy its message into out,
 * acknowledge it with OK, and return 1. Return 0 when no action or a
 * non-send action is outstanding. */
int driver_take_send(raft89 *node, raft89_message *out);

/* Acknowledge every outstanding action with OK. Returns the number of
 * acknowledged actions, or -1 on error. */
int driver_drain(raft89 *node);

/* If the outstanding action is a HARD_STATE with exactly the expected
 * term and vote, acknowledge it with OK and return 1. Otherwise return 0
 * and leave the action outstanding. */
int driver_expect_hard_state(raft89 *node, raft89_term term,
                             raft89_id voted_for);

/* Acknowledge consecutive SEND actions with OK, recording each
 * destination. Returns the number of sends taken. */
unsigned long driver_collect_sends(raft89 *node, raft89_id *to,
                                   unsigned long max);
/* Build semantic test messages. */
void driver_build_vote_request(raft89_id from, raft89_id to, raft89_term term,
                               raft89_index last_index, raft89_term last_term,
                               raft89_message *msg);
void driver_build_vote_response(raft89_id from, raft89_id to, raft89_term term,
                                int granted, raft89_message *msg);
void driver_build_ae(raft89_id from, raft89_id to, raft89_term term,
                     raft89_index prev_index, raft89_term prev_term,
                     raft89_index leader_commit, const raft89_entry *entries,
                     raft89_size entry_count, raft89_message *msg);

/* Perform the durable part of an action on the fake store, emulating a
 * host. LOG_APPEND and LOG_TRUNCATE are supported; other actions are
 * ignored. */
void driver_store_effect(fake_store *store, const raft89_action *action);

/* Peek, perform the durable effect, and acknowledge with OK. */
int driver_ack_with_effect(fake_store *store, raft89 *node);

#endif /* DRIVER_H */
