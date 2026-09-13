# Acceptance: stack exhaustion (A3 triage)

*Relates to `.agent/testing/0013-recursion-exhaustion.md`.*

## MUST

* Invoking a function that recurses non-tail with no base case MUST
  terminate by reporting a `call stack exhausted` trap (`@pass` under
  `assert_exhaustion`). *Verified working.*
* A function that recurses non-tail to a fixed depth and returns MUST
  return its result (no false exhaustion for depths below the limit).
* `assert_exhaustion` commands from the official testsuite MUST pass given
  a sufficiently generous command deadline.

## MUST NOT

* The runtime MUST NOT treat a terminating recursion as exhaustion when it
  is below the recursion limit.
* The runtime MUST NOT change evaluation results for any program that does
  not exhaust the stack.

## Out of scope (perf, not correctness)

* Non-tail recursion is O(n^2) (REC-002): reaching the exhaustion limit
  takes ~30s. This is a performance defect to address separately; it does
  not invalidate the exhaustion semantics above.
