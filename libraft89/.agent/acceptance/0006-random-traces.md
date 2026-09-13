# Acceptance criteria: seeded random traces

Traces to `.agent/testing/0008-random-traces.md`.

## Must exhibit (exhibit)

- The same seed and step count MUST reproduce the same event sequence and
  the same final state.
- A failing trace MUST be printed in the replayable `event TYPE a b u` form.
- `--replay` MUST apply a printed trace and report success when every event
  applies and all invariants hold.
- A sweep of seeds MUST complete without an invariant violation.

## Must reject / fail safely (reject)

- A trace that drives a state violating I01..I20 MUST exit nonzero with the
  trace attached.
- A replay of an event that does not apply to the current state MUST exit
  nonzero rather than silently skipping.
- Replaying a trace with a missing or malformed event line MUST exit nonzero.
