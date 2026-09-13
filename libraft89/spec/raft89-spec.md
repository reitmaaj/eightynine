# libraft89 — specification

Normative contract for libraft89 v1. Sections map to tests under `test/`
and scenarios under `.agent/testing/`. The public surface is
`include/raft89.h`; the crash contract detail is in
`.agent/design/0001-crash-contract.md`.

## 1. Purpose

libraft89 implements the Raft consensus algorithm as a deterministic state
machine. It owns leader election, log replication, commit calculation, and
ordered application. It owns nothing else: no transport, serialization,
threads, clocks, filesystem, database, or application semantics.

## 2. Scope

Included: static membership; opaque entries; one outstanding acknowledged
action; atomic hard-state persistence; prefix-safe log append and truncate;
at-least-once application keyed by log index; diagnostic status.

Excluded: dynamic membership and joint consensus; snapshots and log
compaction; pre-vote; leadership transfer; ReadIndex and leases; wire
encoding; transport; threads; clocks; storage engines.

## 3. State model

Persistent (host-owned, read through `raft89_store`):

```text
current_term   voted_for   log[1..N]
```

Volatile (library-owned, rebuilt on restart):

```text
role   leader_id   commit_index   applied_index
election timer   heartbeat timer   per-peer replication state
```

`RAFT89_ID_NONE` (0) means "no node"; `RAFT89_TERM_NONE` (0) is the initial
term; index 0 precedes the first entry and has implicit term 0.

## 4. Configuration rules

`raft89_create` rejects with `RAFT89_ERR_ARG` unless all hold:

- `config != NULL`, `out != NULL`, `members != NULL`, `member_count >= 1`;
- `self != RAFT89_ID_NONE` and occurs exactly once among the members;
- every member id is non-zero and unique;
- `heartbeat_interval > 0`;
- `election_timeout_min > heartbeat_interval`;
- `election_timeout_max >= election_timeout_min`;
- `max_append_entries > 0` and `max_append_bytes > 0`;
- all five `raft89_store` callbacks and `random.next` are non-NULL.

## 5. Lifecycle and restore

`raft89_create` validates configuration, copies the membership set, reads
`hard_state` and `log_last`, and constructs a follower. It emits no action.
`*out` is `NULL` on every failure path. `raft89_destroy(NULL)` is a no-op.

Restore rules:

- `voted_for` must be `RAFT89_ID_NONE` or a configured member, else
  `RAFT89_ERR_CORRUPT`.
- `log_last` returning `(0, term != 0)` or `(index != 0, 0)` is
  `RAFT89_ERR_CORRUPT`.
- A store read failure returns `RAFT89_ERR_STORE`.
- `commit_index = applied_index = 0` after create, regardless of persisted
  log length.
- The initial election timeout is drawn from the injected random source in
  the inclusive range `[election_timeout_min, election_timeout_max]`.

## 6. Action protocol

Exactly one action may be outstanding. While outstanding, `raft89_tick`,
`raft89_recv`, and `raft89_propose` return `RAFT89_BUSY`.
`raft89_next_action` returns the same action and id until
`raft89_action_done` acknowledges it.

Action ids start at 1, increase monotonically, and are never reused within
one incarnation.

Hard-state changes take effect only when the corresponding `HARD_STATE`
action is acknowledged: while it is outstanding, `status.current_term` and
`status.voted_for` still report the previously acknowledged values. Role,
`leader_id`, and timer changes are volatile and take effect immediately.
The outstanding action itself already carries the pending hard state, so the
host cannot observe an unacknowledged term through the action stream.

`raft89_action_done` accepts `RAFT89_ACTION_OK` for every action,
`RAFT89_ACTION_LOST` only for `RAFT89_ACT_SEND`, and
`RAFT89_ACTION_FATAL` for any action. `LOST` on a non-send action and any
wrong or absent id return `RAFT89_ERR_STATE`. `FATAL` faults the node.

Action semantics:

| Action | Effect | Notes |
|---|---|---|
| `SEND` | transport accepts one message | `LOST` equals packet loss |
| `HARD_STATE` | atomically persist `(current_term, voted_for)` | one unit; never torn |
| `LOG_APPEND` | durably append entries from `last_index + 1` | consecutive |
| `LOG_TRUNCATE` | durably remove entries `>= first_index` | `first_index > commit_index` |
| `APPLY` | apply one committed entry | `index == applied_index + 1` |

Acknowledgement may immediately produce the next action; hosts drain before
submitting another event. Action payloads stay valid until acknowledgement.

## 7. Messages

`raft89_message` is semantic, never a wire representation. Both `from` and
`to` must be configured members; `to` must equal `self`; `from` must not
equal `self`. Malformed input returns `RAFT89_ERR_PROTOCOL` with no state
change and no action.

Field rules:

- `vote_granted` and `success` are exactly 0 or 1;
- a failed `AppendEntries` response carries `match_index = 0`;
- `entry_count > max_append_entries`, aggregate payload bytes
  `> max_append_bytes`, `entries == NULL` with `entry_count != 0`, an entry
  with term 0 or index 0, or a payload pointer `NULL` with size `> 0` are
  protocol errors;
- entries must be consecutive starting at `prev_log_index + 1`, in
  increasing order.

Stale-term requests are rejected without state change. `RequestVote` with a
stale term receives a refusal carrying the current term; `AppendEntries`
with a stale term receives a failure response carrying the current term.

## 8. Election

A follower or candidate whose election timer expires starts an election:
increment the term, vote for itself, persist hard state, then request votes
from every other member. A candidate that receives a quorum of granted
votes becomes leader. A candidate that receives an `AppendEntries` from a
leader of the current term becomes a follower.

Vote granting uses the log-freshness restriction: grant only if the
candidate's last log term is greater, or equal with a last index at least as
large. One vote per term is enforced through the persisted `voted_for`.

A higher-term message always persists the new term before any reply is
emitted; when the vote is refused for freshness, the persisted vote is
cleared to none. A candidate that observes a higher term in a vote response
steps down and persists the new term before any further message.

On acquiring leadership: `leader_id = self`, all `next_index = last_index +
1`, all `match_index = 0`, and an initial heartbeat is sent.

## 9. Replication

The leader appends each proposal at `last_index + 1` with its current term,
persists it, then sends `AppendEntries` to each peer bounded by
`max_append_entries` and `max_append_bytes`. A peer that accepts advances
`match_index`; a peer that rejects backs `next_index` toward the matching
prefix. `match_index` never regresses and a stale response never changes
replication state.

A follower validates `prev_log_index`/`prev_log_term`; a conflict removes
the conflicting suffix (truncate first) and then appends the replacement
entries. A successful response is sent only after those effects are
durable. Identical entries already present are not rewritten.

## 10. Commit and apply

A leader advances `commit_index` only when a quorum of `match_index` values
covers an entry from its current term; older entries commit indirectly.
`commit_index` never decreases. Followers advance `commit_index` from
`leader_commit`, clamped to their last matched entry.

The library emits one `APPLY` at a time for
`applied_index + 1 <= commit_index`, in strictly increasing index order,
only after the entry is durable. Application is at-least-once keyed by
index; repeated applications of one index always carry identical term and
payload. After restart `applied_index = 0`, so replay is expected and the
host deduplicates.

## 11. Timers

`raft89_tick` adds `elapsed` to the logical clocks. A leader emits
heartbeats when the heartbeat interval expires; followers and candidates
start an election when the election timeout expires. A tick that crosses a
timeout triggers at most one transition. Time arithmetic saturates rather
than wrapping.

## 12. Error and fault model

Frozen rules:

- any storage-read failure after creation faults the instance
  (`RAFT89_ERR_FAULTED` thereafter);
- term increment at the maximum value, index arithmetic overflow, and size
  accumulation overflow fault the instance with `RAFT89_ERR_LIMIT` before
  any wrap;
- protocol errors never mutate state and never emit actions;
- allocation failure leaves the previous state intact.

`raft89_status_get` remains readable while faulted; `raft89_destroy` is
always safe.

## 13. Crash contract (summary)

The barrier rule: durable effects may lead acknowledged in-memory state;
acknowledged in-memory state must never lead durable effects. While an
action is outstanding the library keeps the pre-action acknowledged state.

Crash phases (test oracle names): `ISSUED` (C0), `EFFECTING` (CX),
`EFFECT_DONE` (C1), `ACKED` (C2). Allowed crash states per action:

- `HARD_STATE`: old `H0` or complete `H1`, never torn;
- `LOG_APPEND`: `L0` plus any complete prefix of the batch;
- `LOG_TRUNCATE`: suffix removed down to `k-1` at most;
- `SEND`: packet absent or one complete packet accepted;
- `APPLY`: `X0` or complete `apply(X0, e)`, never half-applied.

Restart always yields a follower with `leader_id = NONE`,
`commit_index = 0`, `applied_index = 0`, and no outstanding action.
`restart(C1) == restart(C2)` for completed durable actions; acknowledgement
leaves no durable marker.

## 14. Safety invariants

I01 one leader per term · I02 monotonic terms · I03 one vote per term ·
I04 valid `voted_for` · I05 log matching · I06 leader append-only ·
I07 leader completeness · I08 no divergent applies · I09 ordered apply ·
I10 `applied <= commit <= last` · I11 monotonic commit · I12 monotonic
applied · I13 current-term quorum commit · I14 no vote escape before
durability · I15 no success before durability · I16 no apply before commit ·
I17 one outstanding action · I18 stable action id until ack · I19 monotonic
`match_index` · I20 stale responses cannot regress replication.

## 15. Determinism

Given the same configuration, durable store, random stream, and event
sequence, the library emits exactly the same actions. No global state, no
wall clock, no ambient entropy.

## 16. Test mapping

| Area | Files |
|---|---|
| config, lifecycle, actions | `test/smoke.c`, `test/unit/test_{config,lifecycle,actions}.c` |
| messages, elections, replication, commit, apply, timers | `test/unit/test_*.c` |
| crash points | `test/crash/test_*.c` |
| cluster scenarios and Figure-8 | `test/sim/test_*.c` |
| seeded random traces and replay | `test/sim/test_random_sim.c` |
| bounded model exploration | `test/exhaustive/test_model3.c` |
| allocation failure | `test/failalloc/test_alloc_failure.c` |
| reference host | `examples/host_loop.c` |
