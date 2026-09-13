# Testing: exceptions differential

*BDD scenarios for differential coverage of wasm89 exception handling
(`throw`, uncaught). wasm89 evaluates exceptions on `main`; this adds
cross-engine agreement via an exception template fixture
(`fixtures/ex.wasm`) and exception-status classification.*

## EXC-001 Uncaught throw terminates abnormally in every engine

SCENARIO: Always-throw function
GIVEN an exported function that unconditionally `throw`s a tag with an
    i32 payload
WHEN invoked under wasm89, wasmtime, and wasm-interp
THEN all three terminate abnormally (not returning ok), reported PASS.

## EXC-002 Conditional throw with a normal-return path

SCENARIO: Non-throwing input returns a value
GIVEN an exported function that throws only when its i32 argument is
    nonzero
WHEN invoked with argument `0` under all three engines
THEN all three return ok with the same result `0` (PASS).
SCENARIO: Throwing input
GIVEN the same function invoked with a nonzero argument
WHEN run under all three engines
THEN all three terminate abnormally (PASS).

## EXC-003 Exception status is runtime-failure, not a pass

SCENARIO: wasm89 @exception maps to runtime-failure
GIVEN wasm89 reports `@exception` for an uncaught throw
WHEN compared with an engine that also terminates abnormally
THEN the case is PASS; when compared with an engine that returns ok
    (or errors) it is FAIL/ERROR, never a silent PASS.

## EXC-004 Feature-flag parity

SCENARIO: Interpret runs exceptions with the feature on
GIVEN the exception template fixture (requires the exceptions feature)
WHEN wasm-interp runs it
THEN it is invoked with `--enable-exceptions`.
SCENARIO: wasmtime defaults on
GIVEN the same fixture run under wasmtime
THEN it runs without an extra feature flag (wasmtime enables exceptions).
