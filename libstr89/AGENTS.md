# libstr89 — owning, validated UTF-8 strings for strict C89

A green-compliant, strict-C89 library for owning and editing validated UTF-8
text. `libstr89` owns string storage, byte-offset editing, and exact byte
comparison/search; every Unicode fact (UTF-8 validity, scalar validity,
encoding, boundaries) comes from the sibling `libu89`. It depends on no other
sibling library.

Three types:

- `str89_view` — borrowed, validated UTF-8;
- `str89` — owning, finalized, valid UTF-8;
- `str89_buf` — owning, mutable, valid UTF-8 builder.

Strings preserve their exact UTF-8 byte representation: no normalization, no
case folding, no locale, no collation, no NUL-termination requirement. Byte
length is explicit and embedded U+0000 is an ordinary scalar.

## Hard constraints

- **ISO C89 only**, clean under the green baseline for both GCC and Clang in
  both C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format (`.clang-format`). Verified via the sibling `green`
  driver (`just green` / `just check`).
- **No Unicode codec**: `src/` contains no lead-byte classification,
  continuation checks, surrogate arithmetic, tables, normalization, or case
  folding. `scripts/str89-audit.sh` enforces this and bans NUL-string APIs.
- **Allocator external**: owning values never store their allocator; a NULL
  `str89_alloc` means libc. The destruction allocator must match the
  allocation allocator.
- **Transactional mutators**: a failed mutation leaves the destination
  bit-for-bit unchanged.
- Four-space indentation; functional style; worker/controller structure.

## Public contract

- Public header `include/str89.h`; names are namespaced `str89_*`.
- Offsets are UTF-8 byte offsets; splitting operations require scalar
  boundaries. Scalar boundaries are not grapheme boundaries: grapheme
  semantics remain a `libu89` concern.
- `str89_view_init` is the validation boundary: after it succeeds, a view
  means validated Unicode text.

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` -> failing
test -> minimum code -> refactor. Acceptance tests under `.agent/acceptance/*.md`
cover must-exhibit and must-reject behavior. Coverage order: one end-to-end
smoke test first, then unit tests for every pure function and non-trivial
branch, then fault, model, corpus, and guard-page testing.

## `just` and `make`

Use `just` for all actions: `just build`, `just smoke`, `just unit`,
`just fault`, `just model`, `just corpus`, `just guard`, `just cpp-check`,
`just test`, `just matrix`, `just sanitize`, `just valgrind`, `just green`,
`just check`, `just lint`, `just format`, `just clean`.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
