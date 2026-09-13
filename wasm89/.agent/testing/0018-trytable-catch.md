# Testing: try_table catch-and-return differential

*BDD scenarios for cross-engine differential coverage of `try_table`
catching and returning a value (extends the exception differential of
`0017` with a caught path, not just uncaught throw).*

## TRY-001 Caught throw returns the payload

SCENARIO: Caught payload becomes the result
GIVEN a function whose body may `throw` a tag with an i32 payload, and a
    `try_table (catch $e $handler)` whose `$handler` block yields i32
WHEN invoked with a nonzero argument (so it throws)
THEN wasm89, wasmtime, and wasm-interp all return ok with the argument as
    the result (PASS).

## TRY-002 Normal (non-throwing) path

SCENARIO: No throw takes the fallthrough
GIVEN the same function invoked with argument `0` (so it does not throw)
WHEN run under all three engines
THEN all three return ok with the fallthrough result `-1` (PASS).

## TRY-003 Feature-flag parity

SCENARIO: interpret enables exceptions
GIVEN the try_table fixture
WHEN wasm-interp runs it
THEN it is invoked with `--enable-exceptions` (and wasmtime enables
    exceptions by default), so the case is compared across all three.
