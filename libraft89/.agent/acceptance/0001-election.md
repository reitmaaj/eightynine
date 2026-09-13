# Acceptance criteria: action protocol, timers, elections, votes

Traces to `.agent/testing/0001-election-timers-votes.md`.

## Must exhibit (exhibit)

- Action ids MUST be non-zero, monotonic, and never reused within one
  incarnation; repeated peeks MUST return the identical outstanding action.
- `raft89_tick`, `raft89_recv`, and `raft89_propose` MUST return
  `RAFT89_BUSY` while an action is outstanding.
- `RAFT89_ACTION_LOST` MUST be accepted for `SEND` and rejected with
  `RAFT89_ERR_STATE` for any durable action, leaving it outstanding.
- `RAFT89_ACTION_FATAL` MUST fault the node; `status_get` and `destroy`
  MUST remain safe while faulted.
- An acknowledged action MUST be able to produce the next action
  immediately, observable through `raft89_next_action`.
- Logical time MUST accumulate across ticks and saturate rather than wrap.
  A tick crossing a timeout MUST trigger exactly one election transition.
- A timed-out follower MUST become a candidate, increment the term exactly
  once on durable acknowledgement, vote for itself, and emit `HARD_STATE`
  before any `RequestVote`.
- An election MUST emit exactly one `RequestVote` per other member and none
  to itself.
- A candidate MUST become leader on a quorum of grants (self included) and
  MUST remain a candidate below a quorum; duplicate grants MUST count once
  and refusals MUST not count.
- A one-node cluster MUST become leader immediately after the election hard
  state is acknowledged.
- `RequestVote` freshness MUST follow the lexicographic
  `(last_log_term, last_log_index)` order; an equal-term candidate with a
  shorter log MUST be refused and a longer log granted.
- A repeated request from the already-voted candidate in the same term MUST
  be granted again.
- A higher-term message MUST persist the new term before any reply is
  emitted; when refused for freshness the persisted vote MUST be none.

## Must reject / fail safely (reject)

- A message whose destination is not `self`, whose source is `self` or not
  a configured member, whose type is unknown, or whose vote boolean is not
  0 or 1 MUST return `RAFT89_ERR_PROTOCOL` with no state change and no
  action.
- `RAFT89_ACTION_LOST` on `HARD_STATE` MUST NOT advance acknowledged state.
- An election or vote transition MUST NOT emit a `RequestVote` or a vote
  response before the corresponding `HARD_STATE` acknowledgement.
- A term increment at the maximum representable term MUST fault with
  `RAFT89_ERR_LIMIT` rather than wrap.
