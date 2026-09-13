# libcksum89 - agentic workflow

A green-compliant, standalone ISO C89 library of four fixed checksum
algorithms: CRC-32/ISO-HDLC, CRC-32C, CRC-64/NVME, and the RFC 1071
INET16 Internet checksum. It owns the mathematics and nothing around it.

## Hard constraints

- **ISO C89 only**, clean under the green baseline for both GCC and Clang in
  both C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format (`.clang-format`). Verified via the sibling `green`
  driver (`just green` / `just check`).
- **Standalone**: no sibling headers, no linked sibling library, no libc
  allocation or I/O. The only machine-model requirement beyond C89 is
  `CHAR_BIT == 8`.
- **Leaf boundary**: no algorithm registry, custom polynomials, runtime
  selector, allocation, error/status vocabulary, destroy, fd/`FILE` helpers,
  combine, verify, hex formatting, serialization, endian conversion, SIMD
  selection, plugins, or threads.
- **Caller-owned contexts**: public POD value types, copyable by ordinary
  assignment, never heap-allocated. `final` is `const` and observational.
- Four-space indentation; functional style; most functions short (2-7 lines).

## Public contract

- The public header is `include/cksum89.h`; names are namespaced `cksum89_*`.
- `spec/cksum89-spec.md` is the normative specification; sections map to
  tests under `test/` and scenarios under `.agent/testing/`.
- `cksum89_u16` / `cksum89_u32` name value domains, not object widths.
  `cksum89_u64` is two 32-bit limbs; no C99 64-bit type enters the ABI.
- One-shot functions mean exactly `init; update; final`.
- Pointer misuse is a documented precondition violation, not a status.

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` -> failing
test -> minimum code -> refactor. Acceptance tests under
`.agent/acceptance/*.md` cover must-exhibit and must-reject behavior.
Coverage order: one end-to-end smoke test first, then unit tests for every
pure function and non-trivial branch, then the portability and sanitizer
gates.

## `just` and `make`

Use `just` for all actions: `just build`, `just smoke`, `just unit`,
`just guard`, `just cpp-check`, `just test`, `just sanitize`, `just matrix`,
`just tables`, `just cmake`, `just green`, `just check`, `just lint`,
`just format`, `just doctor`, `just clean`.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
