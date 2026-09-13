# Acceptance criteria: variable watchers (D4)

Additions to `.agent/acceptance/0000-acceptance.md`. Traces to
`.agent/testing/0007-watchers.md` and design `.agent/design/0003-worklist.md`.

## Must exhibit (exhibit)

- `fx_solve()` MUST re-enqueue, on a semantic mutation of a variable, only the
  live PENDING constraints registered as watchers of that variable's
  unresolved tail, and MUST still reach the same fixed point as the reference
  engine.
- A binding that exposes a new unresolved tail MUST (re)register the affected
  constraint as a watcher of the new tail, so later mutations on it propagate.
- Rollback-orphaned constraints MUST be inert: firing their (now stale)
  watcher entries MUST NOT re-enqueue them or reintroduce their semantics.

## Must reject / fail safely (reject)

- Watchers MUST NOT change the semantic answer: no dropped forced consequence
  and no arbitrary branch; scheduling order is not an observable contract.
- Watcher firing MUST NOT resurrect a rolled-back constraint (no consequence
  from a removed constraint may reappear).
- The watcher index MUST NOT expose any new public symbol or engine type.
