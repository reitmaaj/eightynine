#ifndef RAFT89_INTERNAL_H
#define RAFT89_INTERNAL_H

/* raft89_internal.h - private libraft89 representation shared across
 * translation units. Never installed. Not part of the public ABI. */

#include <raft89.h>

/* Private native 64-bit arithmetic. Terms and indices cross the public
 * boundary as {hi, lo} pairs; internally they use the widest compiler type
 * so comparisons and arithmetic stay simple and warning-free. */
__extension__ typedef unsigned long long raft89__u64;

#define RAFT89__U64_MAX (~(raft89__u64)0)

raft89__u64 raft89__from_public(raft89_u64 v);
raft89_u64 raft89__to_public(raft89__u64 v);
int raft89__u64_is_zero(raft89__u64 v);
int raft89__u64_add(raft89__u64 a, raft89__u64 b, raft89__u64 *out);
raft89__u64 raft89__u64_inc(raft89__u64 a);
raft89__u64 raft89__u64_dec(raft89__u64 a);
int raft89__u64_to_size(raft89__u64 v, raft89_size *out);
raft89__u64 raft89__size_to_u64(raft89_size v);

/* Continuation step: what to do after the outstanding action is
 * acknowledged. The step is volatile and is discarded on restart. */
enum raft89_step
{
    RAFT89_STEP_NONE = 0,
    RAFT89_STEP_ELECTION_BROADCAST,
    RAFT89_STEP_VOTE_RESPONSE,
    RAFT89_STEP_STEP_DOWN,
    RAFT89_STEP_AE_HARD_STATE,
    RAFT89_STEP_AE_TRUNCATE,
    RAFT89_STEP_AE_APPEND,
    RAFT89_STEP_AE_REPLY,
    RAFT89_STEP_APPLY,
    RAFT89_STEP_LEADER_APPEND,
    RAFT89_STEP_LEADER_BROADCAST
};

struct raft89
{
    raft89_id self;
    enum raft89_role role;
    raft89__u64 current_term;
    raft89_id voted_for;
    raft89_id leader_id;

    raft89__u64 last_log_index;
    raft89__u64 last_log_term;
    raft89__u64 commit_index;
    raft89__u64 applied_index;

    raft89_id *members;
    raft89_size member_count;
    raft89_id *peers;
    raft89_size peer_count;

    unsigned char *votes;
    raft89_size votes_granted;

    raft89__u64 *next_index;
    raft89__u64 *match_index;

    raft89_time heartbeat_interval;
    raft89_time election_timeout_min;
    raft89_time election_timeout_max;
    raft89_time election_timeout;
    raft89_time election_elapsed;
    raft89_time heartbeat_elapsed;

    raft89_size max_append_entries;
    raft89_size max_append_bytes;

    raft89_store store;
    raft89_random random;

    int faulted;

    raft89_action_id action_seq;
    int has_action;
    raft89_action action;

    enum raft89_step step;
    raft89_size peer_cursor;
    raft89_message pending_msg;

    raft89_entry *pending_entries;
    raft89_size pending_count;
    raft89__u64 ae_prev_index;
    raft89__u64 ae_prev_term;
    raft89__u64 ae_leader_commit;
    raft89_id ae_leader_id;
    raft89__u64 ae_match_index;
    raft89__u64 ae_truncate_first;
    raft89_size ae_append_offset;
    int ae_reply_success;

    void *apply_buffer;
    raft89_size apply_size;
    enum raft89_step after_apply;

    raft89_entry *out_entries;
    raft89_size out_count;
};

/* Pure predicates and helpers. */
int raft89__config_validate(const raft89_config *config);
int raft89__members_valid(const raft89_config *config);
int raft89__timeouts_valid(const raft89_config *config);
int raft89__limits_valid(const raft89_config *config);
int raft89__store_valid(const raft89_config *config);
int raft89__random_valid(const raft89_config *config);
int raft89__config_has_zero(const raft89_config *config);
int raft89__config_has_duplicate(const raft89_config *config);
unsigned long raft89__config_count_id(const raft89_config *config,
                                      raft89_id id);
int raft89__node_has_member(const raft89 *node, raft89_id id);
int raft89__check_restored(const raft89 *node);
int raft89__action_result_valid(const raft89_action *action,
                                enum raft89_action_result result);
unsigned long raft89__sat_add(unsigned long a, unsigned long b);
raft89_size raft89__quorum(const raft89 *node);
raft89__u64 raft89__min_index(raft89__u64 a, raft89__u64 b);
raft89_size raft89__member_index(const raft89 *node, raft89_id id);
int raft89__log_is_fresh(raft89__u64 cand_term, raft89__u64 cand_index,
                         raft89__u64 our_term, raft89__u64 our_index);
int raft89__msg_envelope_valid(const raft89 *node, const raft89_message *msg);
int raft89__request_vote_valid(const raft89_request_vote *rv);
int raft89__vote_response_valid(const raft89_request_vote_response *response);
int raft89__append_entries_valid(const raft89_append_entries *ae,
                                 raft89_size max_entries,
                                 raft89_size max_bytes);
int raft89__append_entries_response_valid(
    const raft89_append_entries_response *response);

/* Lifecycle helpers. */
void raft89__node_init(raft89 *node, const raft89_config *config);
void raft89__node_discard(raft89 *node);
int raft89__node_load(raft89 *node);
int raft89__store_read_hard_state(raft89 *node);
int raft89__store_read_log_last(raft89 *node);
int raft89__recover_applied(raft89 *node, raft89__u64 applied);
raft89_time raft89__pick_timeout(raft89 *node);

/* Action queue. */
int raft89__action_begin(raft89 *node, enum raft89_action_type type);
int raft89__emit_send(raft89 *node, const raft89_message *msg);
int raft89__emit_hard_state(raft89 *node, raft89__u64 term,
                            raft89_id voted_for);
void raft89__apply_hard_state(raft89 *node);
int raft89__resume(raft89 *node);

/* Timers. */
void raft89__advance_clocks(raft89 *node, raft89_time elapsed);
int raft89__tick_leader(raft89 *node);
int raft89__tick_follower(raft89 *node);

/* Election and votes. */
int raft89__start_election(raft89 *node);
int raft89__election_send_next(raft89 *node);
int raft89__become_leader(raft89 *node);
int raft89__step_down_new_term(raft89 *node, raft89__u64 term);
int raft89__recv_request_vote(raft89 *node, const raft89_message *msg);
int raft89__recv_vote_response(raft89 *node, const raft89_message *msg);

/* Leader replication and commit. */
int raft89__leader_broadcast_next(raft89 *node);
int raft89__leader_send_to(raft89 *node, raft89_id peer);
int raft89__leader_maybe_commit(raft89 *node);
int raft89__leader_after_append(raft89 *node);
int raft89__recv_ae_response(raft89 *node, const raft89_message *msg);
int raft89__log_term_at(raft89 *node, raft89__u64 index, raft89__u64 *term);
unsigned char *raft89__entries_buffer_alloc(raft89_size count,
                                            unsigned long total);
/* Message construction. */
void raft89__build_request_vote(const raft89 *node, raft89_id peer,
                                raft89_message *msg);
void raft89__build_vote_response(raft89_id from, raft89_id peer,
                                 raft89_term term, int granted,
                                 raft89_message *msg);
void raft89__build_ae_response(raft89_id from, raft89_id peer, raft89_term term,
                               int success, raft89_index match_index,
                               raft89_message *msg);

/* Log copying, commit, and application. */
int raft89__entries_stash(raft89 *node, const raft89_entry *src,
                          raft89_size count);
void raft89__entries_clear(raft89 *node);
int raft89__advance_commit(raft89 *node, raft89__u64 new_commit,
                           enum raft89_step after);
int raft89__apply_next(raft89 *node);
int raft89__apply_ack(raft89 *node);

/* AppendEntries handling on the follower. */
int raft89__recv_append_entries(raft89 *node, const raft89_message *msg);
int raft89__ae_continue(raft89 *node);
int raft89__ae_after_truncate(raft89 *node);
int raft89__ae_after_append(raft89 *node);
int raft89__ae_send_reply(raft89 *node);

#endif /* RAFT89_INTERNAL_H */
