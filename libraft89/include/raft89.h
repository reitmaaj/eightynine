#ifndef RAFT89_H
#define RAFT89_H

#include <limits.h>

/*
 * raft89.h - deterministic Raft protocol core (ISO C89).
 *
 * libraft89 owns the Raft consensus algorithm and nothing around it.
 * It never performs network I/O, file I/O, serialization, or threading.
 * The host supplies:
 *
 *   - messages in   (raft89_recv)
 *   - commands in   (raft89_propose)
 *   - logical time  (raft89_tick)
 *   - random values (raft89_random)
 *   - storage reads (raft89_store)
 *
 * All externally visible effects leave through raft89_action values:
 * sends, hard-state persistence, log mutations, and state-machine
 * application. Exactly one action may remain outstanding at a time.
 *
 * Persistent state owned by the host:
 *
 *   current_term
 *   voted_for
 *   log[]
 *
 * Volatile state owned by libraft89:
 *
 *   role, leader_id, commit_index, applied_index, timers, replication
 *   state
 *
 * v1 deliberately excludes dynamic membership, snapshots, pre-vote,
 * leadership transfer, ReadIndex/leases, transport, wire encoding,
 * threads, clocks, and filesystem or database integration.
 */

#ifdef __cplusplus
extern "C"
{
#endif

#define RAFT89_VERSION_MAJOR 2
#define RAFT89_VERSION_MINOR 0
#define RAFT89_VERSION_PATCH 0

    /*
     * Portable 32/64-bit scalar types.
     *
     * libraft89 requires an exact unsigned 32-bit C integer type. Terms and
     * log indices are persistent, globally growing 64-bit quantities and are
     * represented portably as {hi, lo} word pairs, independent of the host
     * data model. Arithmetic on them is a private implementation concern.
     */

#if UINT_MAX == 4294967295U
    typedef unsigned int raft89_u32;
#elif ULONG_MAX == 4294967295UL
typedef unsigned long raft89_u32;
#else
#error "libraft89 requires an exact 32-bit unsigned integer type"
#endif

    typedef struct raft89_u64
    {
        raft89_u32 hi;
        raft89_u32 lo;
    } raft89_u64;

    typedef raft89_u64 raft89_term;
    typedef raft89_u64 raft89_index;

    /*
     * Scalar helpers. cmp() returns <0, 0, >0 according to a < b, a == b,
     * a > b.
     */
    int raft89_u64_cmp(raft89_u64 a, raft89_u64 b);

    int raft89_u64_equal(raft89_u64 a, raft89_u64 b);

    raft89_u64 raft89_u64_zero(void);

    raft89_u64 raft89_u64_from_u32(raft89_u32 value);

    /*
     * Scalar types that are not persistent globally growing sequence
     * numbers remain unsigned long.
     */
    typedef unsigned long raft89_id;
    typedef unsigned long raft89_size;
    typedef unsigned long raft89_time;
    typedef unsigned long raft89_action_id;

#define RAFT89_ID_NONE ((raft89_id)0)
#define RAFT89_TERM_NONE (raft89_u64_zero())
#define RAFT89_INDEX_NONE (raft89_u64_zero())
#define RAFT89_ACTION_ID_NONE ((raft89_action_id)0)

    /* Opaque node object. Its representation is private. */
    typedef struct raft89 raft89;

    /* Results. Positive values are states, negative values are errors. */
    enum raft89_result
    {
        RAFT89_OK = 0,

        /* No action is currently outstanding. */
        RAFT89_EMPTY = 1,

        /* The operation requires the leader role. */
        RAFT89_NOT_LEADER = 2,

        /* An action must be acknowledged before another event. */
        RAFT89_BUSY = 3,

        RAFT89_ERR = -1,
        RAFT89_ERR_ARG = -2,
        RAFT89_ERR_NOMEM = -3,
        RAFT89_ERR_STATE = -4,
        RAFT89_ERR_STORE = -5,
        RAFT89_ERR_CORRUPT = -6,
        RAFT89_ERR_PROTOCOL = -7,
        RAFT89_ERR_LIMIT = -8,
        RAFT89_ERR_FAULTED = -9
    };

    enum raft89_role
    {
        RAFT89_FOLLOWER = 0,
        RAFT89_CANDIDATE = 1,
        RAFT89_LEADER = 2
    };

    /*
     * Persistent hard state. current_term and voted_for form one
     * persistence unit. The host MUST persist a RAFT89_ACT_HARD_STATE
     * value atomically: changing the term and clearing or changing the
     * vote must never become separately crash-visible.
     */
    typedef struct raft89_hard_state
    {
        raft89_term current_term;
        raft89_id voted_for;
    } raft89_hard_state;

    /*
     * Log entries. data points to exactly size bytes; size may be zero.
     * Log indices start at 1.
     *
     * Entries supplied to raft89_recv() remain owned by the caller and
     * need remain valid only for the duration of that call.
     *
     * Entries exposed through actions remain owned by libraft89 and remain
     * valid until the corresponding action receives acknowledgement.
     */
    typedef struct raft89_entry
    {
        raft89_term term;
        raft89_index index;
        const void *data;
        raft89_size size;
    } raft89_entry;

    /*
     * Semantic protocol messages, not wire representations. A transport
     * may encode these structures in any format.
     */
    enum raft89_message_type
    {
        RAFT89_MSG_REQUEST_VOTE = 1,
        RAFT89_MSG_REQUEST_VOTE_RESPONSE = 2,
        RAFT89_MSG_APPEND_ENTRIES = 3,
        RAFT89_MSG_APPEND_ENTRIES_RESPONSE = 4
    };

    /*
     * RequestVote. The envelope's "from" field identifies the candidate.
     */
    typedef struct raft89_request_vote
    {
        raft89_term term;
        raft89_index last_log_index;
        raft89_term last_log_term;
    } raft89_request_vote;

    /*
     * RequestVote response. vote_granted must contain either 0 or 1.
     */
    typedef struct raft89_request_vote_response
    {
        raft89_term term;
        int vote_granted;
    } raft89_request_vote_response;

    /*
     * AppendEntries. The envelope's "from" field identifies the leader.
     * entry_count == 0 represents a heartbeat. Entries must contain
     * consecutive indices beginning at prev_log_index + 1 and must appear
     * in increasing index order.
     */
    typedef struct raft89_append_entries
    {
        raft89_term term;
        raft89_index prev_log_index;
        raft89_term prev_log_term;
        raft89_index leader_commit;
        const raft89_entry *entries;
        raft89_size entry_count;
    } raft89_append_entries;

    /*
     * AppendEntries response. success must contain either 0 or 1. When
     * success != 0, match_index names the highest index matched by this
     * AppendEntries RPC. When success == 0, match_index carries no meaning
     * in v1 and must contain RAFT89_INDEX_NONE.
     */
    typedef struct raft89_append_entries_response
    {
        raft89_term term;
        int success;
        raft89_index match_index;
    } raft89_append_entries_response;

    /*
     * Message envelope. Both from and to must name members of the
     * configured cluster.
     */
    typedef struct raft89_message
    {
        enum raft89_message_type type;
        raft89_id from;
        raft89_id to;
        union
        {
            raft89_request_vote request_vote;
            raft89_request_vote_response request_vote_response;
            raft89_append_entries append_entries;
            raft89_append_entries_response append_entries_response;
        } u;
    } raft89_message;

    /*
     * Persistent storage read interface. libraft89 never mutates persistent
     * storage through these callbacks; mutations appear as actions. Every
     * callback returns RAFT89_OK on success or a negative raft89_result on
     * failure. Index 0 has implicit term 0 and never causes entry callbacks.
     */
    typedef struct raft89_store
    {
        void *ctx;

        /* Read persistent current_term and voted_for. */
        int (*hard_state)(void *ctx, raft89_hard_state *state);

        /*
         * Return the last log index and its term. Empty log:
         * *index = 0, *term = 0.
         */
        int (*log_last)(void *ctx, raft89_index *index, raft89_term *term);

        /* Return the term of an existing entry. */
        int (*log_term)(void *ctx, raft89_index index, raft89_term *term);

        /* Return the payload size of an existing entry. */
        int (*log_size)(void *ctx, raft89_index index, raft89_size *size);

        /*
         * Copy exactly size bytes of entry payload into data. size will
         * equal the value returned previously by log_size(). data may be
         * NULL only when size == 0.
         */
        int (*log_read)(void *ctx, raft89_index index, void *data,
                        raft89_size size);
    } raft89_store;

    /*
     * Random source. Return a value in 0 <= value < upper_exclusive.
     * upper_exclusive always exceeds zero. Deterministic tests supply a
     * deterministic implementation.
     */
    typedef unsigned long (*raft89_random_fn)(void *ctx,
                                              unsigned long upper_exclusive);

    typedef struct raft89_random
    {
        void *ctx;
        raft89_random_fn next;
    } raft89_random;

    /*
     * Configuration.
     *
     * members must contain member_count unique non-zero ids, must contain
     * self exactly once, remains caller-owned, and need remain valid only
     * during raft89_create().
     *
     * Time values share one caller-defined unit. Required:
     * heartbeat_interval > 0, election_timeout_min > heartbeat_interval,
     * election_timeout_max >= election_timeout_min.
     *
     * max_append_entries and max_append_bytes bound one AppendEntries
     * message in both directions; both must exceed zero.
     */
    typedef struct raft89_config
    {
        raft89_id self;
        const raft89_id *members;
        raft89_size member_count;
        raft89_time heartbeat_interval;
        raft89_time election_timeout_min;
        raft89_time election_timeout_max;
        raft89_size max_append_entries;
        raft89_size max_append_bytes;
        raft89_store store;
        raft89_random random;

        /*
         * Highest log index which the host knows was durably and
         * successfully applied to its application state machine.
         *
         * Zero means no recovered application checkpoint and preserves the
         * v1 restart behavior. A non-zero value must not exceed the durable
         * last log index; raft89_create() verifies that and probes the
         * durable entry through the store, failing with ERR_CORRUPT or
         * ERR_STORE otherwise.
         *
         * The host MUST supply only an index previously emitted by
         * RAFT89_ACT_APPLY whose application effect became durable before
         * the checkpoint became durable. The library cannot independently
         * prove that condition; it validates the checkpoint against the
         * durable local log only.
         */
        raft89_index applied_index;
    } raft89_config;

    /*
     * Actions. All externally visible effects leave through this
     * interface. Exactly one action may remain outstanding.
     *
     * While an action remains outstanding, raft89_tick(), raft89_recv(),
     * and raft89_propose() return RAFT89_BUSY. raft89_next_action()
     * returns the same outstanding action until raft89_action_done()
     * acknowledges it.
     */
    enum raft89_action_type
    {
        /* Send one semantic Raft message. */
        RAFT89_ACT_SEND = 1,

        /* Atomically persist current_term and voted_for. */
        RAFT89_ACT_HARD_STATE = 2,

        /*
         * Append one or more entries durably to the existing log.
         * entries[0].index must equal the current last index + 1.
         */
        RAFT89_ACT_LOG_APPEND = 3,

        /* Durably remove entries with index >= first_index. */
        RAFT89_ACT_LOG_TRUNCATE = 4,

        /*
         * Apply one committed entry to the application state machine.
         * Applications treat the log index as the stable identity of the
         * application operation.
         */
        RAFT89_ACT_APPLY = 5
    };

    /*
     * Host result for an action.
     *
     * RAFT89_ACTION_OK: effect completed.
     *
     * RAFT89_ACTION_LOST: valid only for RAFT89_ACT_SEND. The message
     * failed to reach the peer; Raft treats this as packet loss.
     *
     * RAFT89_ACTION_FATAL: the effect could not complete safely. The
     * raft89 object enters the faulted state.
     */
    enum raft89_action_result
    {
        RAFT89_ACTION_OK = 0,
        RAFT89_ACTION_LOST = 1,
        RAFT89_ACTION_FATAL = 2
    };

    typedef struct raft89_action_send
    {
        raft89_message message;
    } raft89_action_send;

    typedef struct raft89_action_hard_state
    {
        raft89_hard_state state;
    } raft89_action_hard_state;

    typedef struct raft89_action_log_append
    {
        const raft89_entry *entries;
        raft89_size entry_count;
    } raft89_action_log_append;

    typedef struct raft89_action_log_truncate
    {
        raft89_index first_index;
    } raft89_action_log_truncate;

    typedef struct raft89_action_apply
    {
        raft89_entry entry;
    } raft89_action_apply;

    typedef struct raft89_action
    {
        raft89_action_id id;
        enum raft89_action_type type;
        union
        {
            raft89_action_send send;
            raft89_action_hard_state hard_state;
            raft89_action_log_append log_append;
            raft89_action_log_truncate log_truncate;
            raft89_action_apply apply;
        } u;
    } raft89_action;

    /*
     * Diagnostic state. No field requires host persistence.
     * applied_index <= commit_index <= last_log_index.
     */
    typedef struct raft89_status
    {
        raft89_id self;
        enum raft89_role role;
        raft89_term current_term;
        raft89_id voted_for;
        raft89_id leader_id;
        raft89_index last_log_index;
        raft89_index commit_index;
        raft89_index applied_index;
        int faulted;
    } raft89_status;

    /*
     * Lifecycle.
     *
     * raft89_create() validates configuration, copies the static membership
     * set, reads hard state and log metadata from store, creates a follower,
     * and chooses an election timeout using config.random. No message or
     * storage mutation action results merely from opening an existing node.
     * On failure *out is set to NULL.
     *
     * raft89_destroy(NULL) is a no-op.
     */
    int raft89_create(const raft89_config *config, raft89 **out);
    void raft89_destroy(raft89 *node);

    /*
     * Advance logical time by elapsed units (the unit used by the
     * configuration timeouts). elapsed may equal zero.
     */
    int raft89_tick(raft89 *node, raft89_time elapsed);

    /*
     * Deliver one complete semantic Raft message. The caller may release
     * all memory referenced by message when this function returns;
     * libraft89 copies anything that must survive an action boundary.
     */
    int raft89_recv(raft89 *node, const raft89_message *message);

    /*
     * One application command in a batch proposal.
     */
    typedef struct raft89_command
    {
        const void *data;
        raft89_size size;
    } raft89_command;

    /*
     * Propose one non-empty consecutive batch of application commands.
     * Only the current leader accepts proposals.
     *
     * count must satisfy 1 <= count <= config.max_append_entries.
     *
     * The sum of the command payload sizes must not overflow raft89_size
     * and must not exceed config.max_append_bytes.
     *
     * For each command:
     *
     *     size > 0  => data must not be NULL
     *     size == 0 => data may be NULL
     *
     * On RAFT89_OK:
     *
     *     *first_index receives the index assigned to commands[0]
     *
     * and the commands receive consecutive indices. first_index may be
     * NULL. The library emits one RAFT89_ACT_LOG_APPEND containing the
     * complete batch before any replication SEND for those entries, and no
     * subset of the batch becomes part of the local Raft log.
     */
    int raft89_proposev(raft89 *node, const raft89_command *commands,
                        raft89_size count, raft89_index *first_index);

    /*
     * Propose one application command; equivalent to a one-element
     * raft89_proposev(). On RAFT89_OK, *index receives the assigned log
     * index (the command is not committed until a corresponding
     * RAFT89_ACT_APPLY appears). index may be NULL. data remains
     * caller-owned and need remain valid only for the duration of this
     * call; size may be zero.
     */
    int raft89_propose(raft89 *node, const void *data, raft89_size size,
                       raft89_index *index);

    /*
     * Inspect the outstanding action.
     *
     * RAFT89_OK: *action points to the outstanding action.
     * RAFT89_EMPTY: no action is outstanding and *action becomes NULL.
     *
     * The returned pointer and every pointer reachable through it remain
     * valid until raft89_action_done() or raft89_destroy().
     */
    int raft89_next_action(raft89 *node, const raft89_action **action);

    /*
     * Acknowledge completion of the current action. id must equal the
     * outstanding action id. Processing the acknowledgement may immediately
     * create another action; the host drains actions with
     * raft89_next_action() before submitting another event.
     */
    int raft89_action_done(raft89 *node, raft89_action_id id,
                           enum raft89_action_result result);

    /*
     * Copy current diagnostic state into status. libraft89 v1 supplies no
     * internal synchronization; the caller must not access node from
     * another thread concurrently.
     */
    int raft89_status_get(const raft89 *node, raft89_status *status);

#ifdef __cplusplus
}
#endif

#endif /* RAFT89_H */
