# Design: differential solver oracle (D4/D5 follow-up)

Status: recorded follow-up, not yet implemented. Retains the current
full-rescan saturation loop as an internal *reference* implementation while a
worklist/watcher engine is introduced, and differentially tests that the two
agree on semantics.

## Motivation

The solver worklist changes scheduling only, never semantics. That makes
differential testing unusually strong: the reference and the candidate are
supposed to compute the same fixed point. Any divergence indicates a semantic
regression, not just a timing difference.

## Reference vs candidate

```text
solve_reference(ctx)   current fx_solve loop: repeatedly scan all constraints
                       until fact_serial stops changing
solve_worklist(ctx)    candidate: explicit queue of potentially affected,
                       still-PENDING constraints
```

Both live behind one internal entry so production code has a single
`fx_solve`; the oracle is compiled in only for test builds.

## What the oracle compares

On cloned/generated equivalent problems, after each of
`solve_reference`/`solve_worklist`:

- solve status (`FX_OK` / failure kind);
- normalized observable rows for every row variable;
- membership / lacks / purity / subset query results;
- residual constraint `PENDING`/`SATISFIED`/`FAILED` states;
- last-conflict kind and provenance;
- rollback/re-solve behavior (checkpoint → solve → rollback → solve).

## Cloning

A problem is cloned by replaying the same public construction calls in a
second independent `fx_ctx`. Determinism of ids/order
(`.agent/design/0000-design.md`) makes the two contexts comparable by
observable values rather than pointers.

## Properties to hold

- Same fixed point reachable; extra propagations in one but not the other are
  only allowed if they are *forced* consequences (never arbitrary branching on
  an underdetermined join — `.agent/acceptance/0000-acceptance.md`).
- `PENDING` remains an exact unresolved residual, not a failure signal.
- Worklist ordering does not leak into observable semantics.

## Suggested harness shape

A dedicated test program enumerates small structural cases plus generated
random-but-seeded constraint graphs, clones each, runs both solvers, and
compares the above observables. Randomization is seeded and the corpus is
deterministic across runs to keep `main` reproducible.
