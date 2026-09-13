# BDD scenarios: bounded model checking

Drives `test/unit/test_invariants.c`, `test/unit/test_inspect.c`, and
`test/exhaustive/test_model3.c`. Traces to
`.agent/design/0005-model-checker.md`.

- SCENARIO Valid cluster passes: GIVEN a consistent cluster state WHEN the
  invariant engine runs THEN it reports no violation.
- SCENARIO Two leaders in one term: GIVEN two live leaders whose status
  carries the same term WHEN checked THEN a one-leader violation is reported.
- SCENARIO Vote changes within a term: GIVEN a durable vote in term T WHEN a
  transition changes that vote while the term stays T THEN a one-vote
  violation is reported. Two different nodes voting differently in one term
  is legal and MUST be accepted.
- SCENARIO Divergent apply: GIVEN two applications that recorded different
  payloads at the same index WHEN checked THEN a divergence violation is
  reported.
- SCENARIO Clone equivalence: GIVEN a node and a deep clone WHEN both receive
  the same events THEN their canonical snapshots and actions stay identical.
- SCENARIO Bounded exploration: GIVEN the default bounds WHEN
  `just exhaustive` runs THEN every reachable state satisfies I01..I20 and
  the run reports pass with non-zero leader, log, commit, and apply coverage.
- SCENARIO Seeded roots: GIVEN the deterministic leader, committed-entry, and
  partitioned-follower roots WHEN explored THEN a shallow run still reaches
  commit and apply states.
- SCENARIO State cap: GIVEN a deliberately tiny state cap WHEN exploration
  exceeds it THEN the checker reports `INCONCLUSIVE` and exits 0, or exits 2
  under `--strict`, never pass.
- SCENARIO Self check: GIVEN a hand-broken cluster WHEN the checker's
  `--self-check` mode runs THEN it detects the injected violations, prints
  `SELF-CHECK OK`, and exits 0.
- SCENARIO Replayable trace: GIVEN a failing run WHEN the printed seed and
  event trace are replayed THEN the same violation is reached.
