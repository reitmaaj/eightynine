# Acceptance: iterative machine — calls and frames, non-tail (S2.3)

*Relates to `.agent/testing/0021-iter-calls.md`. Stakeholder value: story
0006. Design: `.agent/design/0007` §2.3, §3, §4. This stage re-enables the
perf gate `test/perf_on2.sh` (0014).*

## MUST

* `call` and `call_indirect` MUST push one `FUNC` level whose frame base
  aligns with the caller's live argument values and whose locals/params are
  copied into a fresh `w89_frame`.
* A `FUNC` level whose pc reaches `return` MUST be popped and MUST leave
  exactly the function's `exit_arity` results above its `base` in the
  caller's value frame; the driver MUST repeat this while the new top is at
  a return.
* Host calls MUST run inline and MUST NOT leave an open `FUNC` level.
* `down(5000)` MUST complete within the `test/perf_on2.sh` O(N) deadline
  with a flat C stack, returning normally (see `0014` acceptance).
* Unbounded recursion MUST trap at the exhaustion budget (5000 open
  non-tail frames) and MUST NOT exhaust the native C stack.
* Only non-tail `FUNC` pushes MUST be charged against the budget; tail calls
  and blocks MUST be uncharged.
* `call`/`call_indirect`/`fac`/`skip-stack-guard-page` conformance MUST be
  green under `W89_ITER=1`, identical to the legacy default.

## MUST NOT

* The migration MUST NOT cause false exhaustion for terminating recursion
  below the budget.
* It MUST NOT change the depth at which `assert_exhaustion` fires for
  genuinely unbounded recursion.
* A call/return MUST NOT corrupt the caller's value frame (lose or duplicate
  results, or leave stray values above the frame base after the call).
* It MUST NOT C-recurse per nesting level: stepping a callee MUST NOT call
  into `step_frame`.
* A wasm `call`/`call_indirect` target (or anything reachable from it) whose
  body has an instruction outside the migrated set MUST NOT be executed
  piecewise by the driver: the invocation MUST fall back to the legacy path
  (or the driver MUST be able to run every reachable body), with a
  bit-identical outcome to the legacy default.
* Budget MUST be charged per simultaneously-open non-tail `FUNC` frame and
  restored on return; it MUST NOT be consumed cumulatively by sequential
  (non-simultaneous) deep recursions below the budget.
* Call `args` MUST be consumed exactly once and the frame base MUST be set
  before any callee body instruction runs.
* The rewrite MUST NOT regress the legacy default path or the full `just
  test` gate.

## Gate

* `test/perf_on2.sh` MUST be re-enabled in `test/run_tests.sh` on this
  stage and MUST pass on the iterative build.
* `just lint ob89 test` green; `call`/`call_indirect` conformance green
  under `W89_ITER=1` and the default, identical outcomes.
* `down(5000)` fast and flat-C-stack; no native-stack-overflow at depth 5000.
