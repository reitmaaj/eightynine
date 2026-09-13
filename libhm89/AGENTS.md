# libhm89 — agentic workflow

A green-compliant, generic Hindley–Milner type inference library written in
strict ISO C89. It is a sibling git repository to `green` (the source-profile
toolchain used to verify it) and the other `vibe` projects.

## Hard constraints

- **ISO C89 only**, clean under the green baseline for both GCC and Clang in
  both C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format. Verified by running the sibling `green` driver
  against generated GCC and Clang compilation databases (`just green`).
- **Primitive generic API as V1 contract**: spec §51 plus the §10 convenience
  helpers. No AST adapter, no rollback, no rigid variables, no constructor
  registry, no built-in types. See `spec/hm-spec.md`.
- No dependency beyond the C89 standard library.
- Context-scoped ownership: objects remain valid until `hm_ctx_destroy()`.
- Four-space indentation; functional style; most functions short (2–7 lines).

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` → failing
test → minimum code → refactor. Acceptance tests under `.agent/acceptance/*.md`
cover must-exhibit and must-reject behavior. Coverage order: one end-to-end
smoke test first, then unit tests for every pure function and non-trivial
branch, then broader testing.

## `just` and `make`

Use `just` for all actions. Recipes are declared in the root `Justfile`.
`just build` compiles, `just test` runs smoke + unit, `just green` runs the
seven-cell green matrix, `just check` is the green gate, `just lint` /
`just format` handle shellcheck + clang-format.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
