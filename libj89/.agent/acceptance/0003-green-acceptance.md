# 0003-green-acceptance

## Must exhibit

- `just green` runs the green toolchain `check` against the four libj89
  translation units and reports CGREEN PASS.
- The compiler matrix (GCC C89, GCC C23, Clang C89, Clang C23), the
  clang-tidy semantic suite (both standards), and the canonical format check
  all PASS with no diagnostics.
- Effectful calls bind their result in a complete assignment statement; no
  effectful call is the direct operand of `return`.
- Null pointer constants are spelled `NULL` (or a variable bound to `NULL`
  in its own statement); no cast is mixed into a call argument.
- Redundant casts between identical canonical types are absent.
- The sources remain strict-C89 clean under both GCC and Clang, and the
  unit/e2e suite still passes after the green refactor.

## Must reject

- `just green` MUST fail (exit non-zero, CGREEN FAIL) if any translation
  unit fails any of the seven green checks.
- A source change that reintroduces `return effectful_call(...)`,
  `buf = malloc(cap)` as a direct RHS (an effectful call not forming a
  complete transition), a bare `0` null pointer constant, or a redundant
  cast MUST be rejected by the green gate.
