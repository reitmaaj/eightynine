# Acceptance criteria: configuration and lifecycle

These criteria cover the M0/M1 increment. They trace to
`.agent/testing/0000-scenarios.md` and are enforced by `test/smoke.c`,
`test/unit/test_config.c`, `test/unit/test_lifecycle.c`, and
`test/unit/test_actions.c`.

## Must exhibit (exhibit)

- `raft89_create` MUST accept valid one- through five-node static
  configurations and produce a follower with `leader_id == RAFT89_ID_NONE`.
- `raft89_create` MUST restore `current_term` and `voted_for` exactly from
  the store and MUST expose `last_log_index`/`last_log_term` from
  `log_last`.
- A successful create MUST leave `commit_index == 0` and
  `applied_index == 0` and MUST emit no action.
- `raft89_status_get` MUST copy the complete diagnostic state without
  mutating the node.
- `raft89_next_action` on a fresh node MUST return `RAFT89_EMPTY` and set
  the output action pointer to `NULL`.
- `raft89_destroy(NULL)` MUST be a no-op.
- `raft89_action_done` with no outstanding action or a wrong id MUST return
  `RAFT89_ERR_STATE`.

## Must reject / fail safely (reject)

- A null configuration, null output pointer, zero or duplicate member id,
  missing or duplicated `self`, zero member count, missing store callback,
  missing random source, zero heartbeat, `election_timeout_min <=
  heartbeat_interval`, `election_timeout_max < election_timeout_min`, or a
  zero append bound MUST return `RAFT89_ERR_ARG` and MUST set `*out` to
  `NULL`.
- A persisted `voted_for` naming a non-member MUST return
  `RAFT89_ERR_CORRUPT`.
- `log_last` returning `(0, N>0)` or `(I>0, 0)` MUST return
  `RAFT89_ERR_CORRUPT`.
- A failing `hard_state` or `log_last` callback MUST return
  `RAFT89_ERR_STORE`; no partially constructed node may escape and `*out`
  MUST be `NULL`.
- No rejected configuration or failed create may emit an action or leak
  library-owned memory.
