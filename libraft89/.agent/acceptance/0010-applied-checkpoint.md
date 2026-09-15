# Acceptance: recovered application checkpoint

## Mandatory behaviors (must exhibit)

- `config.applied_index == 0` preserves the v1 restart behavior: commit and
  applied start at zero and no action is emitted by creation.
- A checkpoint `0 < C <= last_log_index` sets both `applied_index` and
  `commit_index` to `C` and emits no action by itself.
- `applied_index == last_log_index` is valid.
- After restart with checkpoint `C`, the first APPLY is exactly `C + 1`
  unless nothing beyond `C` is committed; entries at or below `C` are never
  applied again.
- An APPLY whose application effect became durable but whose checkpoint did
  not may be re-emitted after restart; the host contract remains
  at-least-once.
- Role transitions never reduce `applied_index` or `commit_index` below the
  recovered checkpoint.

## Unacceptable behaviors (must reject / refuse)

- `config.applied_index > last_log_index` MUST fail creation with
  `RAFT89_ERR_CORRUPT` and `*out == NULL`.
- A checkpoint whose durable entry cannot be confirmed by the store MUST
  fail creation with `RAFT89_ERR_STORE` and `*out == NULL`; the library MUST
  NOT silently accept unverifiable recovered application state.
- Creation MUST NOT emit an APPLY action merely because a checkpoint was
  supplied.
- No code path may apply an index at or below a recovered checkpoint without
  a preceding crash that lost the corresponding checkpoint.
