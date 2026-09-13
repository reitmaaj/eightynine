# BDD scenarios: configuration and lifecycle

Scenarios in this file drive `test/smoke.c`, `test/unit/test_config.c`,
`test/unit/test_lifecycle.c`, and `test/unit/test_actions.c`. IDs
cross-reference the acceptance document `.agent/acceptance/0000-acceptance.md`.

## Configuration validation

- SCENARIO Valid one-node cluster: GIVEN a configuration whose only member
  is `self` WHEN `raft89_create` runs THEN it succeeds (C01).
- SCENARIO Valid two-node cluster: GIVEN two distinct members including
  `self` WHEN created THEN it succeeds (C02).
- SCENARIO Valid three-node cluster: GIVEN three distinct members including
  `self` WHEN created THEN it succeeds (C03).
- SCENARIO Valid four-node cluster: GIVEN four distinct members including
  `self` WHEN created THEN it succeeds (C04).
- SCENARIO Valid five-node cluster: GIVEN five distinct members including
  `self` WHEN created THEN it succeeds (C05).
- SCENARIO Null configuration: GIVEN `config == NULL` WHEN created THEN
  `RAFT89_ERR_ARG` (C06).
- SCENARIO Null output pointer: GIVEN a valid configuration and
  `out == NULL` WHEN created THEN `RAFT89_ERR_ARG` (C07).
- SCENARIO Zero self id: GIVEN `self == 0` WHEN created THEN
  `RAFT89_ERR_ARG` (C08).
- SCENARIO Zero member id: GIVEN any member id `0` WHEN created THEN
  `RAFT89_ERR_ARG` (C09).
- SCENARIO Duplicate member: GIVEN the same member id twice WHEN created
  THEN `RAFT89_ERR_ARG` (C10).
- SCENARIO Self absent: GIVEN `self` not among the members WHEN created
  THEN `RAFT89_ERR_ARG` (C11).
- SCENARIO Self twice: GIVEN `self` occurring twice WHEN created THEN
  `RAFT89_ERR_ARG` (C12).
- SCENARIO Zero members: GIVEN `member_count == 0` WHEN created THEN
  `RAFT89_ERR_ARG` (C13).
- SCENARIO Missing store callback: GIVEN any `raft89_store` callback is
  `NULL` WHEN created THEN `RAFT89_ERR_ARG` (C14).
- SCENARIO Missing random source: GIVEN `random.next == NULL` WHEN created
  THEN `RAFT89_ERR_ARG` (C15).
- SCENARIO Zero heartbeat: GIVEN `heartbeat_interval == 0` WHEN created
  THEN `RAFT89_ERR_ARG` (C16).
- SCENARIO Election minimum not above heartbeat: GIVEN
  `election_timeout_min <= heartbeat_interval` WHEN created THEN
  `RAFT89_ERR_ARG` (C17).
- SCENARIO Election maximum below minimum: GIVEN
  `election_timeout_max < election_timeout_min` WHEN created THEN
  `RAFT89_ERR_ARG` (C18).
- SCENARIO Zero append limits: GIVEN `max_append_entries == 0` or
  `max_append_bytes == 0` WHEN created THEN `RAFT89_ERR_ARG` (C13a).

## Restored persistent state

- SCENARIO Vote for non-member: GIVEN a persisted `voted_for` naming a
  non-member WHEN created THEN `RAFT89_ERR_CORRUPT` and `*out == NULL` (C19).
- SCENARIO Empty log `(0,0)`: GIVEN `log_last` returns `(0,0)` WHEN created
  THEN it succeeds (C20).
- SCENARIO Empty log with term: GIVEN `log_last` returns `(0,N>0)` WHEN
  created THEN `RAFT89_ERR_CORRUPT` (C21).
- SCENARIO Nonempty log with term zero: GIVEN `log_last` returns
  `(I>0,0)` WHEN created THEN `RAFT89_ERR_CORRUPT` (C22).
- SCENARIO Store read failure: GIVEN `hard_state` or `log_last` fails WHEN
  created THEN `RAFT89_ERR_STORE` and no object escapes (C29).
- SCENARIO Output after failure: GIVEN any failed create THEN `*out` is
  `NULL` (C30).

## Lifecycle

- SCENARIO Create performs no action: GIVEN a successful create WHEN
  `raft89_next_action` runs THEN `RAFT89_EMPTY` and output is `NULL` (C23).
- SCENARIO Initial role: GIVEN a successful create THEN role is
  `RAFT89_FOLLOWER` (C24).
- SCENARIO Initial leader: GIVEN a successful create THEN `leader_id` is
  `RAFT89_ID_NONE` (C25).
- SCENARIO Restored term: GIVEN persisted term `T` WHEN created THEN
  `status.current_term == T` (C26).
- SCENARIO Restored vote: GIVEN persisted vote `V` WHEN created THEN
  `status.voted_for == V` (C27).
- SCENARIO Restored log position: GIVEN `log_last` returns `(I,T)` WHEN
  created THEN `status.last_log_index == I` and volatile
  `commit_index == applied_index == 0`.
- SCENARIO Repeated create/destroy: GIVEN many create/destroy cycles WHEN
  run THEN no allocation grows without bound (C28).
- SCENARIO Destroy NULL: GIVEN `node == NULL` WHEN destroyed THEN it is a
  no-op.

## Action protocol basics

- SCENARIO No action: GIVEN a fresh node WHEN `raft89_next_action` runs
  THEN `RAFT89_EMPTY` and `*action == NULL` (A01).
- SCENARIO Acknowledge with no action: GIVEN no outstanding action WHEN
  `raft89_action_done` runs THEN `RAFT89_ERR_STATE` (A08).
- SCENARIO Acknowledge wrong id: GIVEN no outstanding action WHEN
  `raft89_action_done` runs with an arbitrary id THEN `RAFT89_ERR_STATE`
  (A07).
- SCENARIO Null arguments: GIVEN `node == NULL` or `action == NULL` WHEN
  queried THEN `RAFT89_ERR_ARG`.
