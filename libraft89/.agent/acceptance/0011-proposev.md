# Acceptance: batched proposals

## Mandatory behaviors (must exhibit)

- `raft89_proposev()` accepts `1 <= count <= max_append_entries` commands
  whose payload sum does not exceed `max_append_bytes`, and emits exactly
  one `RAFT89_ACT_LOG_APPEND` containing the complete batch before any
  replication SEND.
- Entries receive consecutive indices starting at `last_log_index + 1` and
  carry the leader's current term; `*first_index` names the first entry.
- A zero-length command payload is valid with `data == NULL`.
- A single-node cluster commits an acknowledged batch in order: one
  `LOG_APPEND`, then `APPLY` for each entry ascending.
- `raft89_propose()` remains present and behaves as a one-element
  `raft89_proposev()`.
- A successful proposal means only "accepted into the leader's log
  workflow"; commitment still requires the corresponding APPLY action.

## Unacceptable behaviors (must reject / refuse)

- `count == 0`, `commands == NULL`, or a command with `size > 0` and
  `data == NULL` MUST return `RAFT89_ERR_ARG` with no log mutation, no
  action, and no index reservation.
- `count > max_append_entries` or a payload sum above `max_append_bytes`
  MUST return `RAFT89_ERR_LIMIT` without mutation.
- A saturating sum that overflows `raft89_size` MUST be rejected; it MUST
  NOT wrap to a small value.
- A batch whose indices would pass `2^64-1` MUST fault the node and return
  `RAFT89_ERR_LIMIT` without an action.
- A non-leader MUST return `RAFT89_NOT_LEADER`; a leader with an
  outstanding action MUST return `RAFT89_BUSY`; neither may mutate state.
- On any failure, no subset of the batch may become part of the local Raft
  log and no index may be consumed.
