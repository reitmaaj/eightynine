# BDD scenarios: mutation-trail rollback (D2)

These scenarios drive the mutation trail that replaces the checkpoint
variable/state snapshots. Semantics are unchanged from the snapshot-based
rollback; only the mechanism differs (undo records applied in reverse).

## Rollback of full solver state

- SCENARIO Rollback restores constraint progress: GIVEN a member constraint
  solved (SATISFIED) within a checkpoint THEN rollback returns it to PENDING
  and a later `fx_solve()` repropagates (already covered in 0001).
- SCENARIO Rollback restores conflict: GIVEN a member/lacks clash solved to
  `FX_ERR_UNSAT` within a checkpoint THEN rollback resets the last-conflict
  kind to NONE and removes the speculative constraint.
- SCENARIO Rollback of variable facts and bindings: GIVEN required/forbidden
  facts and a binding added within a checkpoint THEN rollback removes them.

## Nesting and commit

- SCENARIO Nested rollback: GIVEN cp1 then cp2 THEN rolling back cp2 returns
  to the cp1 state (a constraint added after cp1 but before cp2 reverts to
  PENDING); re-solving restores its consequences.
- SCENARIO Commit then rollback rejected: GIVEN cp1 committed THEN rolling
  back cp1 is rejected.
