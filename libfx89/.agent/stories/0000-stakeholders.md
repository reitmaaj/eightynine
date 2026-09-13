# Stakeholders

- AS a compiler writer for a Hindley-Milner language
  I WANT to attach an effect row to each inferred expression and unify rows
  where value types unify
  SO THAT the value-type algebra (`libhm89`) stays separate from the effect
  algebra (`libfx89`).

- AS a compiler writer for an effect-polymorphic language
  I WANT to generalize and instantiate effect-only schemes
  SO THAT a higher-order function such as `map : (a ->{e} b) -> ... ->{e} ...`
  keeps its effect variable `e` parametric across call sites.

- AS a compiler writer enforcing a capability boundary
  I WANT to check `required <= granted` as an effect subset relation
  SO THAT I can reject expressions that demand an ungranted effect.

- AS the author of a Datalog/relational system (Hoodlum-style)
  I WANT to tag foreign-source clauses with an effect while leaving pure
  relational rules with the empty row
  SO THAT the engine decides scan timing while `libfx89` decides whether a
  clause permits or requires the corresponding effect.

- AS the author of an action/effect evaluator (JJ-style)
  I WANT to compose effect requirements by exact set union without committing
  to sequencing or suspension
  SO THAT the action machine remains a runtime concern.

- AS a static analyzer over a dynamically typed language
  I WANT to accumulate effect rows with no static value types at all
  SO THAT `libfx89` is usable without any type-system dependency.

- AS a library maintainer
  I WANT `libfx89` to stay a strict standalone ISO C89 sibling that is
  green-clean and exposes only the `fx_*` surface
  SO THAT it never leaks value semantics and remains reusable by unrelated
  language experiments.

- AS a library maintainer
  I WANT deterministic, context-owned, monotonic solving with checkpoints
  SO THAT lifecycle, incremental inference, and rollback are predictable.

- AS a language front-end author doing speculative inference
  I WANT checkpoint/rollback over solver state
  SO THAT backtracking a failed branch never corrupts the surviving solution.
