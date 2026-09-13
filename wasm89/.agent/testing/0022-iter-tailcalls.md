# Testing: iterative machine — tail calls (S2.4)

*BDD scenarios for migrating `return_call`/`return_call_indirect` onto the
`FUNC` level stack with in-place frame reuse (PLAN S2.4). Stakeholder value:
story 0006. Design: `.agent/design/0007` §2.2, §4.*

## ITT-001 A tail call reuses the frame in place

SCENARIO: Tail call replaces the top frame without growth
GIVEN an open `FUNC` level executing `return_call`/`return_call_indirect`
WHEN the tail call executes under the iterative driver
THEN the current `FUNC` level is replaced by the callee `FUNC` reusing the
    same value frame/base (depth does not grow)
AND no budget charge is incurred for the tail transition.

SCENARIO: Deep tail recursion succeeds
GIVEN a tail-recursive loop expressed via `return_call` that runs 1,000,000
    iterations
WHEN it runs under the iterative driver
THEN it completes successfully without exhaustion and without growing the
    level stack.

SCENARIO: Tail call results match legacy
GIVEN the same tail-call programs
WHEN run under `W89_ITER=1` and under the legacy default
THEN outcomes and results are identical
AND the known `return_call*` conformance set is green.

## ITT-002 Arity and args honoured on frame reuse

SCENARIO: Tail arguments and results respect the frame base
GIVEN a tail call whose callee has `nparams` params and `nresults` results
WHEN the frame is reused
THEN the callee's params are set up from the caller's tail arguments above
    the frame base, and the callee's `exit_arity` results replace them on
    return
AND a caller value frame below the base is untouched.
