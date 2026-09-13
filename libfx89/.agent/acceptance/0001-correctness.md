# Acceptance criteria: correctness pass

Additions to `.agent/acceptance/0000-acceptance.md` for the P0/P1
correctness fixes. They trace to `.agent/testing/0001-join-rollback-purity.md`.

## Must exhibit (exhibit)

- Exact JOIN with **closed** inputs MUST respect the output row's explicit
  head and treat the output as the whole row to be equaled to the exact
  union `normalize(left \u222a right)`. `{A | z} = {A} \u222a {B}` MUST
  succeed with `z = {B}`.
- Rollback MUST restore constraint completion state, not only facts/bindings:
  after rolling back a checkpoint taken before a `fx_solve()`, a previously
  SATISFIED constraint MUST become eligible to propagate again on the next
  `fx_solve()`.
- A failing checkpoint snapshot MUST leave the context unchanged (no token is
  returned, no checkpoint is pushed).
- `fx_row_is_pure` MUST report `FX_FALSE` for a row whose unresolved tail is
  proven nonempty (its representative carries a required atom), using only
  already-derivable facts and no universe enumeration.
- `fx_check_subset(R,R)` MUST report `FX_TRUE` for any row `R`, open or
  closed.

## Must reject / fail safely (reject)

- `{C | z} = {A} \u222a {B}` MUST fail; the solver MUST NOT bind `z` to
  `{A,B}` and thereby accept a row `{A,B,C}` unequal to the union.
- A checkpoint created under snapshot OOM MUST NOT be usable for rollback or
  commit; it must fail safely rather than dereference a null snapshot.
- On allocation failure the solver MUST propagate `FX_ERR_NOMEM`; it MUST NOT
  encode failure as an empty set-difference result, a truncated JOIN support,
  or a false equality.
