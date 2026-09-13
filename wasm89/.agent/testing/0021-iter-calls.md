# Testing: iterative machine — calls and frames, non-tail (S2.3)

*BDD scenarios for migrating `call`/`call_indirect` and the function frame
onto `FUNC` machine-stack levels (PLAN S2.3). This is the stage that flips
the perf gate `test/perf_on2.sh` green (`down(5000)` becomes O(N) on a flat
C stack). Stakeholder value: story 0006. Design: `.agent/design/0007` §2.3,
§3, §4.*

## ITC-001 A non-tail call pushes a `FUNC` level

SCENARIO: Call sets up locals and a frame base
GIVEN a function body executing `call` (or `call_indirect`)
WHEN the call executes under the iterative driver
THEN one `FUNC` level is pushed with the callee `finst`, a fresh
    `w89_frame` holding copied locals/params, and a value-stack `base`
    such that the caller's live args are above the call base
AND the args are consumed and the frame base is where the callee's own
    operand frame begins.

SCENARIO: Callee returns by splicing into the caller frame
GIVEN an open `FUNC` level whose pc has reached `return`
WHEN the driver pops it
THEN exactly the function's `exit_arity` results remain above the frame
    `base` in the caller's value frame
AND the driver repeats while the new top is also at a return.

## ITC-002 Budget bounds open non-tail frames

SCENARIO: Terminating recursion below budget succeeds
GIVEN a non-tail recursion `down(N)` for N < 5000
WHEN it runs under the iterative driver
THEN it completes normally without exhaustion and in time proportional to
    N (O(N), flat C stack) — `test/perf_on2.sh` passes.

SCENARIO: Unbounded recursion traps at the budget
GIVEN recursion that never terminates
WHEN the number of simultaneously open non-tail `FUNC` levels would exceed
    the budget (5000)
THEN the driver reports `call stack exhausted` (or equivalent exhaustion)
    quickly and does not recurse further.

## ITC-003 Host calls run inline and tail/block are uncharged

SCENARIO: Host function executes inline
GIVEN a `call` to an imported host function
WHEN it executes
THEN the host call runs inline (no `FUNC` level is left open on the machine
    stack) and its results are spliced into the caller frame.

## ITC-005 Iter-eligibility falls back to legacy for non-migrated callees

SCENARIO: A call to a callee with a non-migrated body is not iterated
GIVEN a migrated caller body that `call`s a callee whose own body contains
    an instruction outside the migrated set (e.g. a memory/table op or a
    tail call), or that reaches one through `call_indirect`
WHEN the whole invocation is run under the iterative entry
THEN the invocation MUST NOT be run piecewise by the driver; it either runs
    entirely on the legacy path or the driver must be able to execute every
    reachable callee body
AND the outcome is bit-identical to the legacy default.

## ITC-006 Budget bounds simultaneously-open frames, not cumulative work

SCENARIO: Sequential (non-nested) deep recursions do not falsely exhaust
GIVEN an invocation that performs two non-nested non-tail recursions each
    well under the budget
WHEN it runs under the iterative driver
THEN budget is charged per simultaneously-open `FUNC` frame and restored when
    a frame returns, so the total work does not consume the budget
AND only genuine unbounded recursion traps at the exhaustion budget.

## ITC-004 Result parity with legacy and no re-descend

SCENARIO: Call results match legacy
GIVEN the same programs exercising `call`, `call_indirect`, `fac`,
    `skip-stack-guard-page`
WHEN run under `W89_ITER=1` and under the legacy default
THEN outcomes and results are identical.

SCENARIO: No per-step ancestor re-descend
GIVEN a depth-N non-tail recursion running under the iterative driver
WHEN each instruction executes
THEN the driver steps only the top `FUNC` level in place (no O(N)
    surface-to-depth travel per instruction), giving O(N) total work.
