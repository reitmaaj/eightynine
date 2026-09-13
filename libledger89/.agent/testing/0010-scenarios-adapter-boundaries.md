# BDD scenarios: adapter boundaries (L10)

These scenarios drive `test/adapters/` and constrain how a consumer (for
example a replication adapter) may use libledger89 through the public API
alone. They add no library semantics: the library still owns only a durable,
append-only record sequence. IDs cross-reference
`.agent/acceptance/0010-adapter-boundaries.md`.

## Follower append and replay

- SCENARIO Contiguous batches: GIVEN an empty ledger WHEN an adapter appends
  contiguous batches, syncs, closes, and reopens THEN iteration replays the
  exact indices, tags, and payload bytes (AB01).
- SCENARIO Duplicate index: GIVEN `last_index == N` WHEN an index `<= N` is
  appended THEN `LEDGER89_ERR_SEQUENCE` and no record, counter, or byte
  changes (AB02).
- SCENARIO Gap index: GIVEN `last_index == N` WHEN an index `> N + 1` is
  appended THEN `LEDGER89_ERR_SEQUENCE` and the ledger is unchanged (AB03).

## Conflict replacement

- SCENARIO Replace suffix: GIVEN records `1..5` WHEN `truncate_after(2)`
  succeeds and fresh records `3..4` are appended THEN iteration yields `1..4`
  with the fresh suffix, no gap, and no resurrected old content (AB04).
- SCENARIO Below base: GIVEN `first_index > 1` WHEN
  `truncate_after(first_index - 2)` is requested THEN `LEDGER89_ERR_RANGE` and
  the visible range is unchanged (AB05).

## Projection rebuild

- SCENARIO Observer projection: GIVEN an observer-fed projection built over
  successful appends WHEN the handle is reopened and the projection is rebuilt
  from iteration alone THEN the two projections are identical (AB06).
- SCENARIO Late observer: GIVEN appends made before an observer is installed
  WHEN further appends are observed THEN the observer projection is incomplete
  but the iteration rebuild is complete and correct (AB07).
- SCENARIO Advisory only: GIVEN an observer WHEN an append is rejected THEN the
  observer is not called; WHEN cleared with `NULL` THEN later appends do not
  call it (AB08).

## Range discipline

- SCENARIO Reversed iteration range: GIVEN `first > last`, both nonzero WHEN
  `ledger89_iter_open` runs THEN `LEDGER89_ERR_ARG` and `*out` stays `NULL`
  (AB09).
