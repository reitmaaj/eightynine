# BDD scenarios: immutable schemes

These scenarios drive the frozen-template scheme redesign (immutable
meaning, fact/disjoint preservation on instantiation, closure validation).

## Immutability under later mutation

- SCENARIO Generalize then bind source: GIVEN `generalize e` over an
  unconstrained `e` THEN later binding the original `e = {A}` and
  instantiating produces a fresh unconstrained variable, not the
  retroactively-bound `{A}`.
- SCENARIO Generalize then add fact: GIVEN `e` requires `A` at generalization
  time THEN an instantiated fresh variable also requires `A`.

## Instantiation preserves the disjoint-head invariant

- SCENARIO Instantiate disjoint tail: GIVEN body `{A | e}` generalized THEN
  instantiation yields `{A | fresh}` where `fresh` lacks `A`, so requiring
  `A in fresh` is a contradiction.

## Residual closure validation

- SCENARIO Explicit outer reference rejected: GIVEN an explicit scheme whose
  residual constraint references a mutable, unquantified outer variable THEN
  `fx_scheme_new` is rejected.
- SCENARIO Duplicate quantified variable rejected: GIVEN the same variable
  listed twice as quantified THEN `fx_scheme_new` is rejected.
