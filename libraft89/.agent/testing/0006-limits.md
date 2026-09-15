# BDD scenarios: integer and size boundaries

Drives `test/unit/test_limits.c`.

- SCENARIO Term overflow: GIVEN a persisted term at the maximum value WHEN
  an election would increment it THEN `RAFT89_ERR_LIMIT` is returned with no
  state change and the term never wraps. (The v2 policy supersedes the v1
  faulting behavior; see `0011-portable-scalars.md` U05.)
- SCENARIO Index overflow on leadership: GIVEN a log ending at the maximum
  index WHEN a candidate would become leader THEN the node faults with
  `RAFT89_ERR_LIMIT` rather than compute `index + 1`.
- SCENARIO Proposal index overflow: GIVEN a batch that would pass the
  maximum index WHEN proposed THEN the node faults with `RAFT89_ERR_LIMIT`
  and emits no action.
- SCENARIO AppendEntries index overflow: GIVEN `prev_log_index` too close
  to the maximum for the batch length WHEN validated THEN
  `RAFT89_ERR_PROTOCOL` and no state change.
- SCENARIO Payload size overflow: GIVEN entry sizes whose aggregate
  saturates WHEN validated THEN the message is rejected rather than
  wrapping.
- SCENARIO Proposal limit: GIVEN a payload larger than `max_append_bytes`
  WHEN proposed THEN `RAFT89_ERR_LIMIT` and the log is unchanged.
- SCENARIO Faulted persistence: GIVEN a node faulted by an overflow THEN
  further events return `RAFT89_ERR_FAULTED` and status stays readable.
