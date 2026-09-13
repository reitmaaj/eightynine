# Acceptance criteria: randomized reference model (L9)

These criteria trace to `.agent/testing/0009-scenarios-random-model.md` and
are enforced by `test/model/test_random_model.c` (`just model`, `just long`).

## Must exhibit (exhibit)

- For any valid random operation history, the ledger MUST match the
  reference model exactly: `first_index`, `last_index`, every tag and
  payload byte, and the full ascending iteration order.
- A clean reopen MUST restore the live model; a process crash MUST restore
  the live model; a power loss MUST restore exactly the durable model.
- `truncate_after` and `discard_before` MUST follow the model's clamping and
  error rules.
- The same seed MUST reproduce the same history and outcome.

## Must reject / fail safely (reject)

- No operation may produce a gap, a duplicate, an unreadable index, or a
  record whose bytes differ from the model.
- No crash may expose a record beyond the durable model or lose a synced
  record.
- A failing comparison MUST print the seed and operation number so the
  history is reproducible.
