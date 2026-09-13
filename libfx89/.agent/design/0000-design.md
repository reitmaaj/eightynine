# Design

## Module boundaries (source layout)

- `fx_ctx.c` — context lifecycle, arena/object registry, rollback
  generation, isolation metadata.
- `fx_kind.c` — effect-kind registry (define/lookup/name/userdata).
- `fx_op.c` — operation metadata registry under a kind.
- `fx_atom.c` — nominal atoms; later parameterized atoms and the atom domain.
- `fx_var.c` — row variables, ids, debug names, bindings, membership facts.
- `fx_row.c` — row construction, normalization, canonical ordering,
  disjoint-tail enforcement, inspection, membership/purity/closedness,
  union, removal, free variables.
- `fx_constraint.c` — constraint objects (id, kind, state, userdata) and the
  public `fx_require_*` constructors.
- `fx_solve.c` — worklist main loop; variable binding/alias/occurs.
- `fx_solve_equal.c` / `fx_solve_member.c` / `fx_solve_subset.c` /
  `fx_solve_join.c` — per-kind processors (member and lacks share facts.c).
- `fx_facts.c` — required/forbidden atom facts on variables.
- `fx_watch.c` — watcher registration and variable wake-up.
- `fx_scheme.c` — effect-only schemes, generalization, instantiation,
  residual-constraint capture.
- `fx_checkpoint.c` — checkpoint journal and rollback/commit.
- `fx_diag.c` — conflict kinds, last-conflict provenance, version.
- `fx_internal.h` — private types shared across translation units.

The full v1 surface splits across the above as milestones land; green only
requires that every `src/*.c` be lint-clean, not any particular split.

## Core representation

A normalized row is

```text
{head | tail}         head = canonical unique atom set; tail = fx_var* or NULL
```

with the disjoint-tail invariant `head ∩ tail = {}`. Row variables are
union-find representatives with optional structural binding, a required atom
set, a forbidden atom set, watchers, and a version counter.

## Solver architecture

A monotone worklist. Constraints accumulate only between checkpoints. Each
constraint kind has a processor:

- EQUAL uses principal open-row unification (fresh shared tail on open/open).
- MEMBER/LACKS reduce to required/forbidden facts on the resolved tail.
- SUBSET remains a first-class residual relation with forward and backward
  propagation through watchers.
- JOIN (exact `out = left ∪ right`) stays residual; it never picks an
  arbitrary branch for an underdetermined output member.

`fx_solve()` returns `FX_OK` once the worklist saturates; `PENDING` residual
subset/join relations are valid symbolic solutions. Occurs is checked before
any binding; cyclic substitutions are rejected.

## Ownership model

- `fx_ctx` owns every created object and all copied names and copied atom
  parameters; `fx_ctx_free` frees library-owned storage only.
- Host `userdata` is never freed by the library.
- Objects never outlive their context. After rollback, speculative object
  pointers become invalid.
- Contexts are independent; no global semantic state. A context is not
  internally synchronized.

## Nominal identity and determinism

Kinds, operations, variables, and constraints receive context-local ids
starting at 1 (0 = invalid), allocated monotonically and never reused.
Atom ordering is by kind id, then the host atom-domain comparator for
parameters. Equal input therefore yields identical ids, normalized rows, and
residual states across runs.

## Constraint and scheme semantics

Subset and exact-join relations survive generalization as **residual
constraints** attached to an effect-only scheme, so instantiation restores
the full symbolic relation rather than an approximation. Schemes quantify
effect-row variables only, never host value types.
