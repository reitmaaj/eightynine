#ifndef RAFT89_INSPECT_H
#define RAFT89_INSPECT_H

/* raft89_inspect.h - test-only access to the private raft89
 * representation, used by the invariant engine and the bounded model
 * checker. Never part of the library and never installed. */

#include <raft89.h>

typedef struct raft89_inspect_view
{
    enum raft89_role role;
    raft89_term current_term;
    raft89_id voted_for;
    raft89_id leader_id;
    raft89_index last_log_index;
    raft89_term last_log_term;
    raft89_index commit_index;
    raft89_index applied_index;
    raft89_time election_timeout;
    raft89_time election_elapsed;
    raft89_time heartbeat_elapsed;
    unsigned long step;
    unsigned long votes_granted;
    raft89_size member_count;
    raft89_size peer_count;
    int has_action;
    int faulted;
} raft89_inspect_view;

void raft89_inspect_view_get(const raft89 *node, raft89_inspect_view *view);

/* Copy the index-th configured peer's identity and replication state.
 * Returns 0 when index is in range, -1 otherwise. */
int raft89_inspect_peer(const raft89 *node, raft89_size index, raft89_id *id,
                        raft89_index *next_index, raft89_index *match_index);

int raft89_inspect_vote(const raft89 *node, raft89_size index, int *granted);

/* The outstanding action, or NULL when none is outstanding. */
const raft89_action *raft89_inspect_action(const raft89 *node);

unsigned long raft89_inspect_action_seq(const raft89 *node);

/* Deep-copy a node including every library-owned buffer. The clone is
 * independent of the source and can be destroyed normally. */
int raft89_inspect_clone(const raft89 *src, raft89 **out);

/* Point a node's store and random callbacks at a new context. Used after
 * cloning so the copy reads the copied fixture. */
void raft89_inspect_rebind(raft89 *node, const raft89_store *store,
                           const raft89_random *random);

/* Write a canonical byte encoding of everything that can affect future
 * behavior. Returns RAFT89_ERR_LIMIT when cap is too small. */
int raft89_inspect_snapshot(const raft89 *node, unsigned char *buf,
                            unsigned long cap, unsigned long *len);

/* Write a canonical byte encoding of one semantic message. */
int raft89_inspect_message_snapshot(const raft89_message *msg,
                                    unsigned char *buf, unsigned long cap,
                                    unsigned long *len);

#endif /* RAFT89_INSPECT_H */
