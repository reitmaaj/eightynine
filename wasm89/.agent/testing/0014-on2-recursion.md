# Testing: O(N) non-tail recursion (P0)

*BDD scenarios for the perf gate that drives the iterative-machine
rewrite. Current non-tail recursion is O(N^2); the target is O(N).
Stakeholder value: story 0006. Design: `.agent/design/0007`.*

## PERF-001 Non-tail recursion is linear in depth

SCENARIO: Deep terminating non-tail recursion
GIVEN a function `down(N)` that recurses non-tail `N` times and returns
WHEN it is invoked for N = 5000
THEN it completes within a bounded wall-clock time proportional to N
    (a generous O(N) bound, ~< 1 s for an interpreter at 5000 depth)
AND returns normally without exhausting.

CURRENT STATE (failing): non-tail recursion is O(N^2); `down(2000)` takes
~5.4 s and `down(5000)` ~30 s+, so the probe times out / is far over the
O(N) bound. This is the regression gate for the iterative machine.

## PERF-002 Deep recursion must not grow the native C stack unboundedly

SCENARIO: Bounded native stack under recursion
GIVEN a recursion of depth 5000 executed repeatedly
WHEN it runs
THEN it MUST NOT overflow the native C stack (must not crash), because
    frame/label nesting is kept on an explicit (heap) stack, not the C
    stack.

## MACH-001 The machine-stack container is a LIFO of levels over a unified value stack

SCENARIO: Level stack push/pop semantics
GIVEN an empty iterative config owning a level array and a unified value
    stack
WHEN levels are pushed in order A then B, then popped once
THEN the popped entry equals B, one level remains, and the remaining top
    equals A
AND pushing/popping beyond empty fails safely (pop on empty returns 0
    and leaves the stack unchanged).

SCENARIO: Unified value stack appends above a reset watermark
GIVEN a config whose unified value stack has been pushed to n values
WHEN the stack is reset to a lower watermark and values are pushed again
THEN values above the watermark are overwritten in place and the length
    reflects the latest pushes.

SCENARIO: Iterative entry is semantic-equal to legacy before migration
GIVEN a config with nothing yet migrated to the machine stack
WHEN it is evaluated via the iterative entry `w89_eval_iter`
THEN the result equals that of the legacy `w89_eval` on the same program
    (the stage-1 fallback must be a no-op).

