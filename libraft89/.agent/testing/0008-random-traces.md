# BDD scenarios: seeded random traces

Drives `test/sim/test_random_sim.c`. Traces to
`.agent/design/0003-simulator-oracle.md` and the shared cluster support.

- SCENARIO Deterministic seed: GIVEN the same seed and step count WHEN the
  simulator runs twice THEN it takes the same events and reaches the same
  state.
- SCENARIO Invariant checking: GIVEN a random trace WHEN any transition
  violates I01..I20 THEN the run exits nonzero and prints the full event
  trace.
- SCENARIO Replay: GIVEN a printed trace WHEN `--replay` consumes it THEN it
  applies the same events and reaches the same state.
- SCENARIO Replay mismatch: GIVEN a malformed or inapplicable event WHEN
  replayed THEN the run exits nonzero.
- SCENARIO Seed sweep: GIVEN a set of seeds WHEN each runs for a fixed number
  of steps THEN every run passes the invariants.
