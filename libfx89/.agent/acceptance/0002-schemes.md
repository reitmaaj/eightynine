# Acceptance criteria: immutable schemes

Additions to `.agent/acceptance/0000-acceptance.md` for the frozen-template
scheme redesign. Traces to `.agent/testing/0002-schemes-immutable.md`.

## Must exhibit (exhibit)

- A scheme MUST capture a frozen snapshot of its quantified variables, body
  row, and residual constraints at construction time; later binding, solving,
  or adding facts to the original variables MUST NOT change what instantiation
  produces.
- Instantiation MUST allocate a fresh variable per quantified slot and
  recreate that slot's required and forbidden facts.
- Instantiation MUST re-assert the disjoint-head lacks invariant on open
  instance rows: an instance `{A | fresh}` MUST forbid `A` from `fresh`.
- `fx_scheme_new` MUST reject duplicate quantified variables.

## Must reject / fail safely (reject)

- `fx_scheme_new` MUST reject any explicit scheme whose body or residual
  constraint references a mutable, unquantified outer variable.
- The solver MUST report a contradiction when an atom required into an
  instance's fresh variable is forbidden by that variable's disjoint-head
  facts.
