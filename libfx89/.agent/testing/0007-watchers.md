# BDD scenarios: variable watchers (D4)

These scenarios drive precise invalidation: a semantic mutation on a variable
re-enqueues only the constraints that watch that variable's unresolved tail,
instead of all PENDING constraints. Scheduling changes only; semantics are
unchanged from the worklist (D3) and the reference engine.

## Precise dependency firing

- SCENARIO Multi-consumer fan-out: GIVEN `src subset d1` and `src subset d2`
  and `A in src` THEN solving derives `A in d1` and `A in d2` (a fact on one
  variable wakes every PENDING constraint watching it).
- SCENARIO Binding exposes a new tail: GIVEN `e1 subset e2`, then `e2 = {|e3}`,
  then `A in e1` THEN solving derives `A in e3` (re-registration follows the
  binding's new unresolved tail).
- SCENARIO No spurious fan-out: GIVEN constraints unrelated to a mutated
  variable THEN that mutation does not enqueue them (only its watchers fire).

## Rollback isolation

- SCENARIO Dead watcher ignored: GIVEN a constraint added then rolled back
  THEN a later mutation on a shared variable does not resurrect the removed
  constraint or reintroduce its consequence.
