# BDD scenarios: explicit worklist (D3)

These scenarios drive replacing the repeated full-constraint rescan with an
explicit worklist over PENDING constraints with coarse (D3) and later precise
(D4) invalidation. Solver semantics are unchanged: scheduling changes only,
not the reachable fixed point.

## Saturation equivalence

- SCENARIO Saturates like the reference: GIVEN arbitrary equality/subset/
  member/lacks/join constraints WHEN `fx_solve()` runs the worklist THEN it
  reaches the same fixed point the reference rescan would: forced membership/
  lacks/subset consequences hold and genuine residuals remain PENDING.
- SCENARIO Transitive forward: GIVEN `e1 subset e2 subset e3` and `A in e1`
  THEN solving derives `A in e3`.
- SCENARIO Backward: GIVEN `e1 subset e2` and `A not-in e2` THEN solving
  derives `A not-in e1`.

## Incremental and repeated solves

- SCENARIO Late fact repropagates: GIVEN a residual subset solved to a fixed
  point THEN adding a new member and calling `fx_solve()` again propagates it
  without duplicating earlier consequences.
- SCENARIO Deterministic: GIVEN the same problem solved twice THEN the
  observable rows, membership, residual states, and conflicts are identical.

## Non-goals

- Scheduling order may differ from the reference rescan; ordering is not an
  observable contract.
- `PENDING` still denotes an exact unresolved residual, never a failure.
