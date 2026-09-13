# libj89 — agentic workflow

This project provides a minimal, strict-C89-clean JSON parser used as a
dependency by the `wid` sibling project. It is a sibling git repository.

Unicode facts (UTF-8 validate/decode/encode, scalar validity, UTF-16
surrogate classification and pair decode) come from the sibling `libu89`.
`libj89` owns JSON syntax and representation only; `j89_alg` remains free of
any Unicode dependency. Builds link `../libu89/build/libu89.a`; `just deps`
builds it. `just unicode-audit` enforces that no UTF-8 algorithm returns to
`src/`.

## Hard constraints

- **Green-compliant** (sibling `green` toolchain): every translation unit is
  ISO C89, clean under the strict C89∩C23 matrix under gcc and clang, passes
  the `green` clang-tidy semantic suite in both standards, and is canonically
  formatted (Allman). The conformance gate is `just green` / `just check`
  (needs `green.yaml`; `just green-db` regenerates the compile databases).
- JSON subset: object, array, string (escapes, `\uXXXX`, UTF-8), integer,
  `true`/`false`/`null`. **No floats.** `version` in WID documents is `0`.
- Four-space indentation.

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
`just build` compiles, `just test` runs the suite, `just green` runs the
green toolchain gate, `just check` is the green gate, and `just lint` runs
shellcheck + clang-format.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
