# BDD scenarios: randomized reference model (L9)

Scenarios in this file drive `test/model/test_random_model.c`. IDs
cross-reference `.agent/acceptance/0009-random-model.md`.

## Reference model

- SCENARIO Model and ledger agree: GIVEN a random operation history WHEN the
  ledger is compared with the reference model THEN `first_index`,
  `last_index`, every record's tag and payload bytes, and full iteration
  order match exactly (M01).
- SCENARIO Determinism: GIVEN the same seed and operation count WHEN the
  test runs twice THEN the same operations and comparisons occur (M02).

## Operations

- SCENARIO Random append batches: GIVEN random batches of 1..4 records WHEN
  appended THEN the model and ledger agree (M03).
- SCENARIO Random reads: GIVEN a random index WHEN read THEN the result is
  the model's record or `LEDGER89_ERR_NOTFOUND` outside the range (M04).
- SCENARIO Random iteration: GIVEN a random range WHEN iterated THEN the
  ascending sequence matches the model (M05).
- SCENARIO Random rotation: GIVEN random rotations THEN the logical sequence
  is unchanged (M06).
- SCENARIO Random truncation: GIVEN `truncate_after(N)` THEN the model keeps
  `<= N`, and below `base - 1` returns `LEDGER89_ERR_RANGE` (M07).
- SCENARIO Random discard: GIVEN `discard_before(N)` THEN the model keeps
  `>= N` with the clamped base (M08).
- SCENARIO Random sync: GIVEN a sync THEN the durable model equals the live
  model (M09).

## Reopen and crash

- SCENARIO Clean reopen: GIVEN any state WHEN closed and reopened THEN the
  live model is restored exactly (M10).
- SCENARIO Process crash: GIVEN unsynced appends WHEN the process exits and
  reopens THEN the live model (page cache) is restored (M11).
- SCENARIO Power loss: GIVEN unsynced appends WHEN power is lost and the
  ledger reopened THEN the durable model is restored exactly (M12).
- SCENARIO Long run: GIVEN millions of operations WHEN the model test runs
  THEN no divergence is found (M13).
