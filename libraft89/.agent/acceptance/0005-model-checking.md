# Acceptance criteria: bounded model checking

Traces to `.agent/testing/0007-model-checking.md` and
`.agent/design/0005-model-checker.md`.

## Must exhibit (exhibit)

- The invariant engine MUST accept a consistent cluster state.
- The invariant engine MUST report one-leader, one-vote, log-matching,
  divergent-apply, and barrier violations when the corresponding state is
  inconsistent; a durable vote that changes within one term MUST be reported
  while two nodes voting differently in one term MUST be accepted.
- `raft89_inspect_clone` MUST produce a node that, driven with the same
  events, emits the same actions and canonical snapshot as the original.
- `just exhaustive` MUST explore the bounded state space from the seeded
  roots and report pass when no reachable state violates I01..I20, with
  non-zero coverage of leaders, durable logs, commits, and applies.
- A violation MUST print the seed and a replayable event trace and exit
  nonzero.
- A run that reaches the state or depth cap MUST report `INCONCLUSIVE`; it
  MUST NOT report pass.

## Must reject / fail safely (reject)

- The checker MUST NOT report pass when the state cap was reached.
- The checker MUST NOT hang on the agreed bounds; the cap bounds the run.
- The checker MUST NOT accept a state where acknowledged in-memory state
  leads durable effects, where a granted vote is visible before its
  `HARD_STATE` acknowledgement, or where `applied > commit`.
- The invariant engine MUST NOT be vacuous: `test/unit/test_invariants.c`
  MUST include one negative case per implemented check.
