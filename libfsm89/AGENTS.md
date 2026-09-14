# libfsm89 — deterministic transition systems for strict C89

A green-compliant, strict-C89, dependency-free library for a small
deterministic labelled transition system with optional state-exit, edge, and
state-entry emissions. One abstraction covers recognizers, Moore/Mealy
transducers, protocol machines, parsers, and effect-driving controllers.

## Hard constraints

- **ISO C89 only**, clean under the green baseline for both GCC and Clang in
  both C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format (`.clang-format`). Verified via the sibling `green`
  driver (`just green` / `just check`).
- **No dependencies**: `libfsm89` references no sibling library and no libc
  symbol; `scripts/audit.sh` enforces the allocation- and I/O-free symbol set.
- **Effects are data**: the library never executes, interprets, or stores
  callbacks. A transition table carries effect identifiers only.
- **No allocation and no hidden state**: everything is caller-owned and
  immutable; there is no mutable FSM object and no `destroy()`.
- Four-space indentation; functional style; worker/controller structure.

## Public contract

- Public header `include/fsm89.h`; names are namespaced `fsm89_*`.
- `fsm89_step` is pure evaluation: it never mutates caller state and writes
  `*result` only on `FSM89_OK`.
- `fsm89_validate` proves structural consistency and determinism only;
  reachability, terminality, completeness, and domain-specific effect
  correctness are separate analyses.
- A successful transition emits `leave(source) || edge || enter(destination)`;
  startup emits `enter(initial)`; a self-transition performs both leave and
  enter.
- All returned effect spans borrow storage from the caller's definition.

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` -> failing
test -> minimum code -> refactor. Acceptance tests under
`.agent/acceptance/*.md` cover must-exhibit and must-reject behavior. Coverage
order: one end-to-end smoke test first, then unit tests for every pure
function and non-trivial branch, then model and generated/exhaustive testing.

## `just` and `make`

Use `just` for all actions: `just build`, `just smoke`, `just unit`,
`just model`, `just generated`, `just deep-test`, `just cpp-check`, `just test`,
`just matrix`, `just sanitize`, `just coverage`, `just audit`,
`just api-coverage`, `just green`, `just check`, `just release`, `just lint`,
`just format`, `just clean`.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
