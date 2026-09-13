# Acceptance: exceptions (Phase D)

*Acceptance criteria for the Phase D milestone. Reference: the exceptions
proposal's evaluation semantics as implemented by the reference
interpreter's `eval.ml` (`Throw`, `ThrowRef`, `TryTable`, and the
`Handler`/`Throwing` administrative instructions), and the `assert_exception`
script command.*

## MUST

* `throw` MUST resolve the tag by index in the current module instance,
  pop the tag's parameter count values from the operand stack in order,
  and raise an exception carrying that tag instance and the payload.
* An uncaught exception at the top level of an invocation MUST surface as
  the exception result status (distinct from a return, a trap, and
  exhaustion) carrying the tag instance and payload.
* `throw_ref` on a non-null exnref MUST re-throw the referenced exception
  (same tag instance and payload); on a null reference it MUST trap with
  "null exception reference".
* `try_table` MUST act as a block for its body (normal completion produces
  the block's results; `br 0` targets the try_table's end; `br k` targets
  the enclosing label), and MUST run the body inside a handler.
* A thrown exception reaching a handler MUST be matched against the catch
  clauses in declared order: `catch $e` on tag-instance identity with the
  payload pushed, `catch_ref $e` with the payload pushed and an exnref on
  top, `catch_all` with no payload, `catch_all_ref` with an exnref. The
  first matching clause MUST branch to its label with the label arity
  enforced by validation.
* An exception matching no catch clause MUST propagate outward through the
  enclosing labels, frames, and handlers (including across calls and tail
  calls) and, if uncaught, end the invocation with the exception status.
* A trap MUST NOT be converted into an exception by a handler and MUST
  propagate with its message intact.
* Catch matching MUST use tag-instance identity so an imported tag aliases
  the exporting module's tag instance.
* The REPL MUST report an uncaught exception from `invoke` with an
  `@exception` response line, distinct from `@return`, `@trap`, and
  `@exhaustion`.
* The REPL MUST accept `assert_exception <name> <func> <nargs> <arg>*` and
  answer `@pass` iff the invocation ends with the exception status.
* The driver MUST process `assert_exception` commands (not skip them) and
  MUST count a command failed when the invocation does not throw.
* The driver MUST count an `assert_invalid` (and `assert_malformed`)
  module that loads successfully as a failure, never as a pass.
* Every `w89_exn` created by a `catch_ref`/`catch_all_ref` match MUST be
  owned by the store and freed at store teardown; no leaks or
  double-frees, including the payload transferred from the throwing
  instruction.

## MUST NOT

* The evaluator MUST NOT trap an uncaught exception or report it as a
  return.
* A catch clause MUST NOT fire for a mismatched tag, and MUST NOT discard
  the payload of a `catch`/`catch_ref` match.
* `catch_all` MUST NOT push the payload; `catch_ref`/`catch_all_ref` MUST
  push exactly one exnref on top of any payload.
* The evaluator MUST NOT apply `skip`/end-consumption of a try_table when
  control leaves the body by exception or outer branch (the end opcode is
  consumed only on normal completion).
* The driver MUST NOT attribute `assert_exception` failures to
  out-of-scope causes once exceptions are enabled.

## Conformance target (Phase D)

The exceptions conformance set (`throw`, `throw_ref`, `try_table`) MUST
pass with zero failures: `throw` 13/13, `throw_ref` 15/15, `try_table`
65/67 (the two skips are text-only `assert_malformed` quote modules,
out of scope for a binary-only runtime). The full spec sweep MUST show no
regression in Phase A/B/C files: 257 files, 36991 passed, 0 failed,
28197 skipped. All unit tests MUST pass under GCC and Clang (strict C89)
and be valgrind-clean.
