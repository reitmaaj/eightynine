# Testing: memory64 and multi-memory differential

*BDD scenarios for cross-engine differential coverage of 64-bit linear
memory (`memory64`) and more than one linear memory (`multi-memory`).*

## MEM64-001 memory64 round-trip

SCENARIO: 64-bit addressing round-trip
GIVEN a module with `(memory i64 1)` and a function that stores an i32 at
    an i64 address and loads it back (address masked in bounds)
WHEN invoked under wasm89, wasmtime, and wasm-interp
THEN all three return ok with the same value (PASS).

## MM-001 Second-memory round-trip

SCENARIO: Store/load through memory 1
GIVEN a module with two memories and a function that stores and loads
    through `(memory 1)`
WHEN invoked under all three engines
THEN all three return ok with the same value (PASS).
SCENARIO: Memory-0 path unaffected
GIVEN the same module storing/loading through the default memory 0
WHEN invoked under all three engines
THEN all three agree (PASS).

## MEM-002 Feature-flag parity

SCENARIO: interpret and wasmtime get the feature flags
GIVEN a memory64 / multi-memory fixture
WHEN the sweep runs it
THEN wasm-interp is invoked with `--enable-memory64` /
    `--enable-multi-memory` and wasmtime with `-W memory64` /
    `-W multi-memory`, and all three engines agree.
