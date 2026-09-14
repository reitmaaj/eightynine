# libledger89 green conformance status

`just green` reports **CGREEN PASS**: all seven checks pass on the current
sources.

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

## Gate

`just check` runs the green matrix, shellcheck/clang-format lint, and the
full fast test suite. The behavioral suites (`just test`, `just sanitize`,
`just valgrind`, `just long`, `just adapters-raft`) pass as well.
