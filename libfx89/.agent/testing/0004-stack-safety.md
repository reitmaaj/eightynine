# BDD scenarios: stack safety (D1)

These scenarios drive the removal of linear recursion from stack-bounded
traversals. No data-model or solver-semantics change is intended; only the
iteration strategy differs.

## Deep traversals terminate

- SCENARIO Long representative/binding chain: GIVEN a chain of many bound row
  variables WHEN a solver or inspection walk dereferences it THEN it
  terminates and returns the correct result rather than overflowing the call
  stack.
- SCENARIO Long atom interning list: GIVEN many distinct parameterized atoms
  sharing a context WHEN a later equivalent atom is interned THEN the linear
  scan terminates and reuses the equivalent atom.
- SCENARIO Many sequential removals: GIVEN a row and many atoms to remove WHEN
  `fx_row_remove_many` runs THEN it terminates and removes every atom.
- SCENARIO Long scheme residual scan: GIVEN a large residual constraint list
  WHEN generalization scans it THEN it terminates and captures the eligible
  residuals without deep recursion.

## Non-goals

- This scenario set makes no claim about performance beyond terminating on
  deep input; algorithmic scaling is handled in D2-D6.
- No representative/union-find, watcher, worklist, or trail semantics are
  introduced here.
