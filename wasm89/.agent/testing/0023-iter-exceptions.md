# Testing: iterative machine — exceptions (S2.5)

*BDD scenarios for migrating `try_table`/`throw`/`throw_ref` and exception
unwinding onto the level stack (PLAN S2.5). A `try_table` becomes a
`BLOCK`-like level carrying catch arms; an in-flight throw unwinds open
levels to the nearest matching catch or surfaces as an exception result.
Stakeholder value: story 0006. Design: `.agent/design/0007` §2.2, §2.3,
§6. Cross-reference: exceptions testing `0010`/`0017`/`0018`.*

## ITX-001 `try_table` becomes a level carrying catches

SCENARIO: try_table pushes a catch-carrying block level
GIVEN a `try_table` with known catch arms
WHEN the driver enters it under `W89_ITER=1`
THEN one `BLOCK`-like level is pushed carrying the `catches` arms, with the
    correct `end`/`exit_arity`/`base`.

SCENARIO: Normal completion splices results
GIVEN a `try_table` body that completes without throwing
WHEN it ends normally
THEN the level is popped and its `exit_arity` results are spliced above its
    `base`, as an ordinary block.

## ITX-002 Throwing unwinds to the nearest matching catch

SCENARIO: Unwind to an enclosing matching catch
GIVEN a thrown tag inside nested open levels with an enclosing `try_table`
    that catches that tag
WHEN the `throw`/`throw_ref` executes
THEN open levels are unwound to the nearest matching catch and the catch's
    continuation runs with the exception payload delivered per the catch
    arm
AND the value/ref/exn payload is preserved.

SCENARIO: Unmatched throw surfaces as an exception result
GIVEN a throw whose tag matches no enclosing catch
WHEN it executes under the iterative driver
THEN it surfaces from evaluation as an exception result (`W89_EVAL_*
    EXCEPTION` path), identical to the legacy default.

SCENARIO: Throw results match legacy
GIVEN the same `throw`/`throw_ref`/`try_table` programs
WHEN run under `W89_ITER=1` and under the legacy default
THEN the resulting pass/trap/exception outcome and payload are identical.

## ITX-003 Unwind honours arity and payload effects

SCENARIO: Catch/rethrow payload order preserved
GIVEN catch arms that capture a non-empty payload (values/refs)
WHEN they fire
THEN the payload values are delivered in the exact order specified by the
    catch arm, matching the reference and the legacy stepper.
