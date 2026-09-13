# Testing: exceptions (Phase D)

*BDD scenarios for the Phase D milestone: the exceptions proposal's eval
surface (`throw`, `throw_ref`, `try_table` and its catch clauses), the
uncaught-exception result status, exception propagation through calls and
frames, exnref values from catch_ref/catch_all_ref, tag-identity catch
matching (including imported tags), and the `assert_exception` test
command.*

## EXN-001 throw and uncaught exception status

SCENARIO: Uncaught throw
GIVEN a module with a tag and a function that `throw`s it
WHEN the function is invoked
THEN evaluation ends with the exception status carrying that tag instance
    (not a return, not a trap).

SCENARIO: Throw payload
GIVEN a tag with parameters and a `throw` providing matching values
WHEN the function is invoked and the exception propagates uncaught
THEN the exception carries those values in order.

## EXN-002 throw_ref

SCENARIO: Rethrow a caught exception
GIVEN an exnref obtained from a `catch_ref`/`catch_all_ref`
WHEN `throw_ref` executes on it
THEN the same tag instance and payload are thrown again.

SCENARIO: throw_ref on null
GIVEN a null reference
WHEN `throw_ref` executes
THEN evaluation traps with "null exception reference".

## EXN-003 try_table catches

SCENARIO: catch tag match
GIVEN a `try_table (catch $e $h)` and a `throw $e` in the body
WHEN the exception is thrown
THEN control branches to the `$h` label with the tag's payload values
    pushed (results selected by the label arity).

SCENARIO: catch tag mismatch
GIVEN a `try_table (catch $e $h)` and a `throw $e2` ($e2 != $e) in the
    body
WHEN the exception is thrown
THEN it is not caught here and continues to propagate outward.

SCENARIO: catch_all
GIVEN a `try_table (catch_all $h)` and any `throw` in the body
WHEN the exception is thrown
THEN control branches to `$h` with no payload pushed.

SCENARIO: catch_ref / catch_all_ref
GIVEN a `catch_ref $e $h` (resp. `catch_all_ref $h`) and a matching throw
WHEN the exception is thrown
THEN an exnref value is pushed on top of the payload and control branches
    to `$h`.

SCENARIO: First matching clause
GIVEN a `try_table` with several catch clauses
WHEN an exception is thrown
THEN the first clause matching the tag (or the first `catch_all`/
    `catch_all_ref` encountered) is taken, in declared order.

## EXN-004 try_table as a block

SCENARIO: Normal completion
GIVEN a `try_table (result t)` whose body completes without throwing
WHEN execution reaches the end
THEN the body's result values are produced like a block.

SCENARIO: Branch out of the body
GIVEN a `try_table` body containing `br 0` / `br k` to an outer label
WHEN the branch executes
THEN it targets the try_table's own end / the outer label respectively,
    with the label's arity enforced.

## EXN-005 Propagation through calls and frames

SCENARIO: Throw in a callee
GIVEN a function that throws and is called from inside a `try_table` body
WHEN the exception is thrown
THEN it propagates through the callee's frame and is caught by the
    handler in the caller.

SCENARIO: Uncaught through a call chain
GIVEN a throw deep in a call chain with no matching handler
WHEN the top-level invoke runs
THEN the invocation ends with the exception status.

SCENARIO: Trap vs exception
GIVEN a `try_table` body that traps (e.g. `unreachable`, integer divide by
    zero)
WHEN the trap propagates
THEN it remains a trap ("unreachable executed" / "integer divide by
    zero"), not an exception, and is not caught by catch clauses.

SCENARIO: return_call in a try_table body
GIVEN `return_call`/`return_call_indirect` inside a `try_table` body whose
    callee throws
WHEN the exception is thrown
THEN it propagates through the tail call and is handled normally.

## EXN-006 Tag identity

SCENARIO: Imported tags
GIVEN a module importing a tag and a second module exporting that tag
WHEN the importing module throws via its (aliased) tag and catches it with
    the imported tag index
THEN the catch matches by the underlying tag instance identity.

## EXN-007 assert_exception harness

SCENARIO: assert_exception pass/fail
GIVEN a driver command `assert_exception` on a function
WHEN the function throws
THEN the harness reports pass.
WHEN the function returns normally or traps
THEN the harness reports fail.

SCENARIO: invoke reports exceptions distinctly
GIVEN a plain `invoke` whose function throws uncaught
WHEN the REPL responds
THEN the response is an exception marker, distinct from `@return`,
    `@trap`, and `@exhaustion`.

## EXN-008 Driver correctness

SCENARIO: assert_invalid must fail on accepted modules
GIVEN an `assert_invalid` command for a module that the validator accepts
WHEN the driver processes it
THEN the command is counted as failed (not passed), so validator misses
    are never masked.
