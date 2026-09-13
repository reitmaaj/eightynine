# libfx89 - specification

Standalone companion to `libhm89` (value types) and `libadt89` (ADTs): the
effect-domain algebra. This document records the public contract and the
normative invariants of `libfx89`. Sections map to tests under `test/` and
scenarios under `.agent/testing/`.

## Purpose

Represent, constrain, and solve finite/open **effect rows** independent of any
value type system, AST, runtime, or effect-execution semantics.

An effect row is a finite set of effect atoms plus an optional open tail
variable:

```text
{}
{IO}
{IO, State}
{IO, State | e}
```

Rows are extensional: `{State, IO}` equals `{IO, State}`, and duplicates
collapse.

## Scope

Supported: nominal effect kinds and operation metadata; nominal effect atoms;
closed and open rows; row variables; effect-only polymorphism; equality,
subset, membership, lacks, and exact-set-union constraints; substitution;
normalization; generalization; instantiation; a monotone solver with
checkpoints and diagnostics.

Excluded (a host interprets these): value types, terms, ASTs, JSON, runtime
values, evaluation order, continuations, handlers, capabilities, host calls,
serialization, async, and resource lifetime. `libfx89` is standalone: it
depends on no sibling library.

## Core model

A normalized row has the form

```text
{head | tail}         head finite canonical atom set; tail = fx_var * or NULL
```

with the **disjoint-tail invariant**: for every atom `a` in `head`, `a` is
forbidden from the (unresolved) tail `e`. This gives every open row a unique
canonical decomposition `explicit finite head + disjoint remainder`.

## Invariants

- I1 Canonical heads: row heads hold unique, canonically ordered atoms.
- I2 Disjoint tails: `{H|e}` implies `H ∩ e = {}` (forced as lacks facts).
- I3 No substitution cycles: variable bindings form a DAG (occurs check).
- I4 Fact consistency: a representative never holds the same atom in both
  required and forbidden sets.
- I5 Monotonic solving: facts and constraints only accumulate between
  checkpoints.
- I6 Stable atoms: atom equality and order never change during a context
  lifetime.
- I7 Exact residuals: pending SUBSET and JOIN constraints keep their full
  mathematical meaning; they are not approximations and never branch on
  underdetermined information.
- I8 No universe enumeration: the solver reasons concretely only about
  encountered atoms.

## Constraint semantics

### Equality

Open/open equality uses principal structural unification: for `{H1|e1} =
{H2|e2}` with `L = H1-H2`, `R = H2-H1`, introduce a fresh shared tail `e3` and
bind `e1 = {R|e3}`, `e2 = {L|e3}`. Open-to-closed binds the tail to the
remaining closed set and fails if an unmatched head cannot be discharged.
Shared tails require equal heads. Cycles are rejected.

### Membership and lacks

`a in R` and `a not-in R` reduce to required/forbidden facts on the resolved
tail; a head membership/lacks clash is an immediate contradiction.

### Purity and closedness

Purity is exact equality to `{}` and legitimately binds an unresolved tail to
empty. Closedness is a validation property only: it is queried (true /
UNKNOWN) and never binds a tail.

### Subset

`LHS ⊆ RHS` remains a first-class residual relation. It propagates every
definite member of LHS into RHS and every definite absence from RHS back into
LHS, persisting across incremental solves.

### Exact join

`out = left ∪ right` is an exact first-class relation (JOIN). It propagates
members forward and out, absence from the union to both operands, and forces
a member onto the other operand only when one operand provably lacks it. An
output member with no input evidence stays UNKNOWN.

## Scheme semantics

A scheme holds quantified row variables, a body row, and residual subset/join
constraints connected to the generalized component. Generalization quantifies
`free(body) - nongeneralizable`; instantiation allocates fresh variables and
re-creates the captured residual constraints as live constraints in the
current context. Schemes quantify effect-row variables only, never value
types.

## Ownership and context rules

An `fx_ctx` owns every object it creates (kinds, operations, atoms, rows,
variables, constraints, schemes, copied names, copied atom parameters, last
conflict). `fx_ctx_free` frees library-owned storage only; host `userdata` is
never freed. Objects do not outlive their context; rolled-back speculative
pointers become invalid. Contexts are independent; there is no mutable global
semantic state. A context is not internally synchronized.

## Determinism

Ids are nonzero, monotonic, never reused, and identical under identical
declaration order. Atom ordering is by kind id then host atom-domain
comparator. Repeating a scenario yields identical normalized rows, residual
states, and diagnostics.

## Failure contract

Fallible calls return an `fx_status`. No semantic error is reported through
`errno`. A context remains inspectable after failure. Invalid inputs that can
be detected safely return `FX_ERR_INVALID`. Unresolved exact symbolic
difference returns `FX_ERR_UNSUPPORTED`. Contradictions return
`FX_ERR_UNSAT`; cycles `FX_ERR_OCCURS`.
