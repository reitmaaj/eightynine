# Acceptance criteria: stack safety (D1)

Additions to `.agent/acceptance/0000-acceptance.md`. Traces to
`.agent/testing/0004-stack-safety.md`.

## Must exhibit (exhibit)

- Representative resolution, occurs/binding walks, parameterized-atom
  interning, `fx_row_remove_many`, and scheme residual scans MUST run to
  completion on input chains deep enough to overflow a recursive
  implementation, without exhausting the call stack and without changing the
  returned result or semantics.

## Must reject / fail safely (reject)

- A deep traversal MUST NOT change any solver or scheme semantics: it MUST
  produce the same observable rows, membership/lacks/purity/subset results,
  residual `PENDING` states, and conflict behavior as a shallow traversal of
  the equivalent problem.
- These rewrites MUST NOT introduce any new public symbol, data model, or
  engine type (representatives, union-find, watchers, worklists, trail
  positions, scratch allocation).
