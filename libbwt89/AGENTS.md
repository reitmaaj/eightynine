# libbwt89 — agentic workflow

A green-compliant, dependency-free library of Burrows–Wheeler transform
variants (regular `bwt`, bijective `bbwt`, dynamic incremental edits) written
in strict ISO C89. It is a sibling git repository to `green` (the source-profile
toolchain used to verify it) and the other `vibe` projects.

## Hard constraints

- **ISO C89 only**, clean under the green baseline for both GCC and Clang in
  both C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format. Verified by running the sibling `green` driver
  against generated GCC and Clang compilation databases (`just green`).
- **Linear-time core algorithms**: suffix-array construction via SA-IS (O(n)),
  LF-mapping inverse (O(n)), Duval Lyndon factorization (O(n)), and Salson
  incremental dynamic edits (no full recompute). No naive quadratic rotation
  sort.
- **Length-prefixed caller-owned byte buffers**: functions take
  `const unsigned char *`, explicit `size_t`, and write into caller buffers.
  Transforms allocate O(n) scratch internally and report `BWT89_NOMEM` on
  failure. Public lengths are capped by `BWT89_MAX_N`; larger `n` returns
  `BWT89_TOO_LARGE` before allocation or input access. Exact in-place
  (`out == s` / `out == b`) is supported for every transform. Binary data,
  including embedded NUL bytes, is supported.
- No dependency beyond the C89 standard library.
- Four-space indentation; functional style; most functions short (2–7 lines).
- No function-like macros; no owned type names ending in `_t`.

## Public API names

- Regular: `bwt89_bwt` (forward), `bwt89_ibwt` (inverse). Forward reports the
  0-based row `index`; no sentinel is added.
- Bijective: `bwt89_bbwt` (forward), `bwt89_ibbwt` (inverse). No index.
- Dynamic: `bwt89_*` edit operations that maintain the transform incrementally.
- All functions return a `bwt89_status`.

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
`just clean`.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
