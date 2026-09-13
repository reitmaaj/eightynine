# libraft89 design

## Module boundaries

- `raft89_config.c` — pure configuration validation helpers.
- `raft89_node.c` — lifecycle, restore from store, status, and the event
  entry points that dispatch into the protocol.
- `raft89_action.c` — the one-outstanding-action queue, action ids,
  acknowledgement, and continuations.
- `raft89_message.c` — envelope validation and message dispatch.
- `raft89_election.c` — follower/candidate transitions, RequestVote.
- `raft89_replication.c` — follower AppendEntries handling, conflict
  repair, and replies.
- `raft89_leader.c` — proposal, replication broadcast, response handling,
  and the current-term commit rule.
- `raft89_timer.c` — logical-time accounting and timeout selection.
- `raft89_log.c` — entry staging, store read helpers, commit advancement,
  and the ordered apply loop.
- `raft89_internal.h` — private representation shared across translation
  units; never installed.

## Core representation

```text
struct raft89
    self, role, current_term, voted_for, leader_id
    last_log_index, last_log_term, commit_index, applied_index
    members[], member_count
    heartbeat_interval, election_timeout_min/max
    election_timeout, election_elapsed, heartbeat_elapsed
    max_append_entries, max_append_bytes
    store, random
    faulted
    action_seq, has_action, action
```

Leader-only peer state exists only while the node is leader. It is
volatile and is never persisted.

## State machine model

Every public entry point constructs an internal event and runs the same
transition:

```text
(state, event) -> (state', actions)
```

Events: `TICK`, `MESSAGE`, `PROPOSE`, `ACTION_DONE`. The transition may
require a durable effect before it can continue; in that case it emits one
action and parks a continuation. The continuation runs only after the host
acknowledges that action. This is how the library enforces durability
barriers without owning storage.

## One-outstanding-action protocol

- `raft89_next_action` returns the current action or `RAFT89_EMPTY`.
- While an action is outstanding, `tick`, `recv`, and `propose` return
  `RAFT89_BUSY`.
- `raft89_action_done` requires the exact outstanding id and accepts `OK`
  for every action, `LOST` only for `SEND`, and `FATAL` for any action.
- Acknowledgement may immediately produce the next action; hosts drain
  before submitting another event.
- Action ids start at 1, are monotonic, and are never reused within one
  incarnation.

## Error model

- `RAFT89_ERR_ARG` — invalid argument or configuration.
- `RAFT89_ERR_NOMEM` — allocation failure; the node is unchanged.
- `RAFT89_ERR_STORE` — a storage read failed; the node faults.
- `RAFT89_ERR_CORRUPT` — restored persistent state violates an invariant.
- `RAFT89_ERR_PROTOCOL` — malformed message; no state change, no action.
- `RAFT89_ERR_LIMIT` — integer or size bound would be exceeded; the node
  faults rather than wrap consensus state.
- `RAFT89_ERR_FAULTED` — the node is faulted; only `status_get` and
  `destroy` remain useful.

Frozen rules: any storage-read failure after creation faults the instance;
any arithmetic overflow in term, index, or size accumulation faults the
instance with `RAFT89_ERR_LIMIT`; `APPLY` is at-least-once keyed by index.

## Determinism

No wall clock, no ambient entropy, no global state. The random source is
injected; logical time is supplied by the caller. Equal inputs and an equal
event sequence produce identical actions.

## Safety invariants

I01 one leader per term · I02 monotonic terms · I03 one vote per term ·
I04 valid `voted_for` · I05 log matching · I06 leader append-only ·
I07 leader completeness · I08 no divergent applies · I09 ordered apply ·
I10 `applied <= commit <= last` · I11 monotonic commit · I12 monotonic
applied · I13 current-term quorum commit · I14 no vote escape before
durability · I15 no success before durability · I16 no apply before commit ·
I17 one outstanding action · I18 stable action id until ack · I19 monotonic
`match_index` · I20 stale responses cannot regress replication.
