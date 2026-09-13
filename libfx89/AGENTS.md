# libfx89 — agentic workflow

A green-compliant, standalone C89 library for nominal finite/open effect
rows, effect-row variables, effect-only polymorphism, and generic constraints
over those rows. It is a sibling git repository to `libhm89`, `libadt89`, and
`green`. Unlike `libadt89`, `libfx89` depends on **no** sibling library.

## Hard constraints

- **ISO C89 only**, clean under the green baseline for both GCC and Clang in
  both C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format (`.clang-format`). Verified via the sibling `green`
  driver (`just green` / `just check`).
- **Standalone**: `libfx89` includes no sibling headers and never links
  `libhm89` or `libadt89`. It reasons only about effect atoms, rows, row
  variables, constraints, and effect-only schemes.
- **No effect execution semantics**: no handlers, continuations, capabilities,
  host dispatch, serialization, or runtime values. A host interprets the
  effect information.
- **No host value types**: object `userdata` and atom parameters are opaque.
  The library never inspects them.
- Four-space indentation; functional style; most functions short (2-7 lines).

## Solver semantics (normative)

- Rows are canonical finite/open sets `{head | tail}` with the **disjoint
  tail invariant**: every explicit head atom is forbidden from the tail.
- Open/open equality uses principal structural unification with a fresh
  shared tail.
- Equality, subset, membership, lacks, and **exact join** constraints are
  primitive and residual: `fx_solve()` returning `FX_OK` may leave genuine
  symbolic relations `PENDING`.
- The solver never enumerates the atom universe and never branches on an
  underdetermined join (output member with no input evidence stays UNKNOWN).
- Cyclic substitutions are rejected (occurs check).
- The solver is deterministic and monotonic between checkpoints.

## Context / ownership

- An `fx_ctx` owns everything it creates (kinds, operations, atoms, rows,
  variables, constraints, schemes, copied names, copied atom parameters, last
  conflict). `fx_ctx_free` frees library-owned storage only; host `userdata`
  is never freed.
- Objects do not outlive their context; rolled-back speculative pointers
  become invalid.
- A context has no internal synchronization; do not share one context across
  threads without external locking.

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` -> failing
test -> minimum code -> refactor. Acceptance tests under `.agent/acceptance/*.md`
cover must-exhibit and must-reject behavior. Coverage order: one end-to-end
smoke test first, then unit tests for every pure function and non-trivial
branch, then broader testing.

## `just` and `make`

Use `just` for all actions: `just build`, `just smoke`, `just unit`,
`just test`, `just green`, `just check`, `just lint`, `just format`,
`just doctor`, `just clean`.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
