# Design: explicit worklist (D3) and watchers (D4)

## Goal

Replace the reference loop (walk every constraint, repeat while `fact_serial`
changed) with a queue of `PENDING` constraints, keeping semantics identical.
The worklist establishes the scheduling machinery; a later, precise watcher
index (D4) narrows invalidation without changing the design.

## Data model

- Each `fx_constraint` carries `qnext` and an `enqueued` bit (context-owned
  linked queue; no allocation on enqueue).
- `fx_ctx` holds the queue head/tail and a `solving` flag gating notification
  so mutations performed while not solving do not enqueue (the next solve
  re-seeds from `PENDING`).

## Algorithm

```text
fx_solve:
    solving = 1
    enqueue every PENDING constraint            # seed
    while queue nonempty:
        c = dequeue()                            # clears c.enqueued
        if c.state != PENDING: continue
        process(c)                               # solve_one
        (propagate any returned error)
    solving = 0
```

## Invalidation (coarse, D3)

Every semantic mutation goes through one of the D2 trail points:
`fx__var_require`/`fx__var_forbid` (fact added) or `bind_row` (binding
installed). Each calls `fx__solve_notify(ctx)`, which — while `solving` —
enqueues **all** still-`PENDING` constraints. The `enqueued` bit dedups.

## Invariants

- Only `PENDING` constraints are processed; a `SATISFIED`/`FAILED` constraint
  dequeued later is skipped (`continue`).
- Processing is monotone in live state. The queue drains because a real
  mutation is recorded once (fact/binding idempotence checks), so re-processing
  a residual eventually stops producing new mutations.
- Scheduling order is not an observable contract; the reachable fixed point is.
- Rollback/trail semantics (D2) are unaffected: the worklist never mutates
  semantic state itself, it only schedules.

## Watchers (D4, next)

Key dependency index on the actual `fx_var *` (not representatives):

```text
EQUAL  a.tail, b.tail   SUBSET  subset.tail, superset.tail
MEMBER row.tail         LACKS   row.tail        JOIN out.tail,left.tail,right.tail
```

`mutation(var)` enqueues `watchers(var)`. Watchers are context-monotonic and
stale entries are harmless (firing only enqueues a constraint that the solver
renormalizes), so no removal or rollback of watchers is required. A binding
exposing a new tail re-registers the new dependency.

## Differential verification

Retain the reference rescan as an internal oracle for tests
(`.agent/design/0002-solver-oracle.md`): generated problems are solved by both
the reference and the worklist and compared on status, normalized rows,
membership/lacks/purity/subset, residual states, conflicts, and rollback.
