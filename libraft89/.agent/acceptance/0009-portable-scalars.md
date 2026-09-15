# Acceptance: portable 64-bit scalars

## Mandatory behaviors (must exhibit)

- `raft89_u64_zero` returns `{0, 0}` and `raft89_u64_from_u32` returns
  `{0, value}`.
- `raft89_u64_cmp` returns `<0`, `0`, `>0` according to mathematical order,
  and `raft89_u64_equal` is exact for the whole 64-bit domain.
- Terms and indices above 2^32 survive creation, status queries, store
  callbacks, messages, and actions without truncation.
- Elections increment terms across the 2^32 boundary exactly.
- `RAFT89_TERM_NONE` and `RAFT89_INDEX_NONE` remain zero.
- The library builds warning-free and passes its tests on an ILP32 target as
  well as LP64.

## Unacceptable behaviors (must reject / refuse)

- Starting an election at term `2^64-1` MUST return `RAFT89_ERR_LIMIT` with no
  state change and MUST NOT wrap to zero.
- Proposing at log index `2^64-1` MUST fault the node and return
  `RAFT89_ERR_LIMIT` without emitting an action.
- No arithmetic on terms or indices may truncate to 32 bits, wrap silently, or
  depend on the host `unsigned long` width.

## ILP32 verification (2026-09-15)

Host: Fedora 43, GCC 15.3.1, `cc -m32` multilib present.

- `just build32` — `build32: ok`.
- `just test32` — the whole fast suite (smoke, 16 unit, 5 crash, 2 sim, and
  the allocation-failure sweep) passes under `-m32`, all linking
  `build/32/libraft89.a`.
- `just sanitize32` — the same suites plus the failalloc sweep under
  `-m32 -fsanitize=address,undefined`; passes with GCC 15.3.1 (i686
  `libasan`/`libubsan`) and with clang 21.1.8 (`just CC=clang sanitize32`,
  `compiler-rt.i686`).
- `just valgrind32` — every 32-bit fast-suite binary under valgrind 3.27.1
  memcheck (`valgrind.i686`); no errors or definite leaks.

`just check` (the seven-cell green matrix) remains the LP64 release gate; the
32-bit recipes are additive and skip cleanly when the toolchain is absent.
