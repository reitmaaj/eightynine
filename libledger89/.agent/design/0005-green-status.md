# libledger89 green conformance status

`just green` reports **CGREEN PASS**: all seven checks pass on the current
sources. `just release-check` passed end to end on 2026-09-15 from a clean
tree (base commit `44dc2f0`, worktree fingerprint `6df7c889...`); the run is
recorded in `.agent/acceptance/0003-rc1-report.md`.

```text
            C89    C23
GCC         PASS    PASS
Clang       PASS    PASS
clang-tidy  PASS    PASS
format      PASS
CGREEN      PASS
```

## What was done

- Short-circuit conditions were rewritten as nested `if` statements or
  explicit boolean locals; `?:` was replaced with `if/else` assignments.
- Calls used in expression position were bound to locals before being tested
  or returned.
- Computed assignments inside nested control blocks were extracted into
  small worker functions; read-only helpers were listed under
  `pure_functions` in `green.yaml`.
- Redundant casts were removed and the canonical Allman format was applied.
- The RC pass added operation-local integrity wording, a physical iterator
  cursor, recovery-time manifest GC, the portability contract, and the
  release gate; every change keeps the matrix green.

## Gate

`just check` runs the green matrix, shellcheck/clang-format lint, and the
full fast test suite. `just release-check` additionally runs full strict,
sanitizers, Valgrind, ratio, coverage, adapters, the 32-bit probe,
benchmarks, and the long tiers from a clean tree. The behavioral suites
(`just test`, `just sanitize`, `just valgrind`, `just long`, `just strict`,
`just strict-long`, `just adapters-raft`) pass as well.
