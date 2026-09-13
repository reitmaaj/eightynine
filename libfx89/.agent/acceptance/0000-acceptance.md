# Acceptance criteria

Acceptance tests encode behavior `libfx89` MUST exhibit and behavior that
would be UNACCEPTABLE (must be rejected / fail safely / be reported as
errors). They trace to `.agent/testing/0000-scenarios.md`, the solver rules
in `AGENTS.md`, and `spec/fx89-spec.md`.

## Must exhibit (exhibit)

- An `fx_ctx` MUST create and destroy; `fx_ctx_free(NULL)` MUST be a no-op.
- Effect-kind and operation registries MUST assign nonzero, monotonic,
  non-reused context-local ids and MUST copy caller names.
- Rows MUST normalize to canonical unique head sets in deterministic atom
  order.
- Open rows MUST carry the disjoint-tail invariant: each explicit head atom
  is forbidden from the unresolved tail (I2).
- `A in {A,B}` MUST report true; `C in {A,B}` MUST report false; membership
  over an unresolved open tail MUST report UNKNOWN, never false.
- Closed equality, open-to-closed binding, and open/open principal
  unification MUST behave as specified; `fx_solve()` MUST return `FX_OK`
  after these derivations.
- Membership, lacks, and purity constraints MUST propagate to required and
  forbidden facts; `require_pure(e)` MUST bind `e` to `{}`.
- Subset constraints MUST remain exact residual relations and MUST propagate
  both forward (member in source to superset) and backward (absence in
  superset to source), persisting across repeated `fx_solve()` calls.
- A symbolic variable under a closed upper bound MUST accept members within
  the bound and reject members outside it WITHOUT enumerating the atom
  universe (I8).
- Exact join MUST preserve set-union semantics and MUST propagate forward,
  negative, and reverse-forced consequences.
- `fx_row_union` and `fx_require_join` MUST never degrade exact union to two
  subset constraints.
- `fx_solve()` MUST be allowed to return `FX_OK` with residual `PENDING`
  subset/join relations (C8).
- Removal MUST compute exact structural difference and MUST report
  `FX_ERR_UNSUPPORTED` rather than guessing when the difference is unresolved.
- Effect-only schemes MUST generalize free (minus nongeneralizable) row
  variables, retain residual subset/join constraints, and instantiate with
  fresh variables plus re-created live constraints (C9).
- Checkpoints MUST roll back solver-domain mutations (facts, bindings,
  aliases, constraints, residuals) while preserving kinds, operations, and
  atom interning (C11).
- Diagnostics MUST report a conflict kind and the participating public
  constraint(s).
- Re-running an identical scenario in independent contexts MUST produce
  identical ids, normalized rows, residual states, and conflicts.

## Must reject / fail safely (reject)

- Cyclic substitutions MUST be rejected as occurs before any binding is made
  (I3).
- A variable MUST NOT hold the same atom in both required and forbidden
  sets; such a clash MUST fail as member/lacks (I4).
- A requires-lacks clash introduced by an open-row head MUST be rejected:
  `A not-in {A|e}` MUST fail.
- A shared-tail open/open equality with different heads MUST fail.
- An equality that would drop an unmatched head atom (e.g. `{A,C|e} =
  {A,B}`) MUST fail.
- Membership of an atom absent from a closed row MUST fail; lacks of an atom
  present in a closed row MUST fail.
- Query APIs (`row_equal`, `row_membership`, `row_is_pure`, `row_is_closed`,
  `check_subset`, `check_lacks`) MUST NOT mutate semantic state; closedness
  MUST NOT bind an unresolved tail (C10-ish).
- The solver MUST NOT arbitrarily branch on an underdetermined join output
  member; that member MUST stay UNKNOWN (I7).
- Invalid inputs (NULL mandatory pointers, invalid ids, mixed-context
  objects, non-top checkpoint tokens) MUST return `FX_ERR_INVALID` where the
  contract permits detection, without corrupting the context.
- `fx_row_remove` on an unresolved symbolic difference MUST return
  `FX_ERR_UNSUPPORTED`, never a guessed row.
- An explicit scheme capturing a constraint that references a mutable
  unquantified outer variable MUST be rejected.
- `libfx89` MUST contain no effect-execution surface: no `fx_eval`,
  `fx_handle`, `fx_resume`, `fx_dispatch`, `fx_capability_*`, or any
  value-type interface. Every exported semantic symbol MUST begin `fx_`
  (C14, namespace rule).

## Conformance boundary

An implementation conforms when it: maintains canonical disjoint-tail rows;
solves equality, subset, membership, lacks, and exact join over open rows;
derives only forced consequences and rejects cycles/contradictions; keeps
residual relations exact through solving and scheme generalization; and rolls
solver state back transactionally. It must never interpret a program value or
value type, and must never execute an effect. Each requirement is covered by
at least one scenario and by tests under `test/`.
