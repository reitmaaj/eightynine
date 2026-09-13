# Acceptance criteria: leader replication and commit rule

Traces to `.agent/testing/0003-leader-replication.md`.

## Must exhibit (exhibit)

- `raft89_propose` MUST assign the next sequential index, use the current
  term, emit `LOG_APPEND` before any replication, and return
  `RAFT89_NOT_LEADER` off the leader.
- An acknowledged local append MUST be replicated to every peer; an
  unacknowledged one MUST NOT be replicated.
- A new leader MUST send an initial `AppendEntries` to every peer.
- A successful response MUST advance that peer's replication state only
  when its `match_index` is higher than the recorded value; a failed
  response MUST back `next_index` toward the matching prefix and retry.
- Batches MUST respect `max_append_entries` and `max_append_bytes`, and a
  caught-up peer MUST receive an empty heartbeat.
- A leader MUST never truncate its own log during one tenure.
- A higher-term response MUST make the leader step down and persist the
  new term before any further message.
- Commit MUST advance only to an index replicated on a quorum whose entry
  belongs to the leader's current term; a quorum of old-term entries alone
  MUST NOT advance commit.
- Once a current-term entry commits, all preceding entries commit with it.
- The leader's own log MUST count once toward the quorum; duplicate
  acknowledgements MUST count once.
- `commit_index` MUST never decrease.

## Must reject / fail safely (reject)

- A proposal on a non-leader MUST return `RAFT89_NOT_LEADER`.
- A proposal while an action is outstanding MUST return `RAFT89_BUSY`.
- A proposal larger than `max_append_bytes` MUST return `RAFT89_ERR_LIMIT`
  without changing the log.
- A response from a non-member, or with an invalid success value, MUST
  return `RAFT89_ERR_PROTOCOL` with no state change.
- A storage failure while building replication batches MUST fault the node
  rather than emit a partial message.
