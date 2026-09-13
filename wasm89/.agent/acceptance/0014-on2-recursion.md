# Acceptance: O(N) non-tail recursion (P0)

*Relates to `.agent/testing/0014-on2-recursion.md` PERF-001/002.*
*Stakeholder value: story 0006. Design: `.agent/design/0007`.*

## MUST

* `down(5000)` (terminating non-tail recursion to depth 5000) MUST
  complete within the bounded O(N) deadline in `test/perf_on2.sh` (well
  under the current O(N^2) ~30 s), and MUST return normally (no
  exhaustion, no crash).
* Repeated depth-5000 recursion MUST NOT overflow the native C stack.
* The rewrite MUST NOT change evaluation results: the full `just test`
  gate and the conformance sweep MUST remain green (no semantic
  regression across the 35k-passing subset).

## MUST NOT

* The rewrite MUST NOT cause false exhaustion for terminating recursion
  below the budget (5000).
* The rewrite MUST NOT change the depth at which `assert_exhaustion`
  fires for genuinely unbounded recursion.
* The rewrite MUST NOT alter evaluation order, trap behavior, exception
  propagation, or multi-value results.

## Gate
* `test/perf_on2.sh` MUST fail on the current O(N^2) build and pass after
  the iterative-machine rewrite.
