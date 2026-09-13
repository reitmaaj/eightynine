# Acceptance: try_table catch-and-return differential

*Acceptance criteria for `try_table` catch-and-return differential coverage
(scenarios `0018`). Test tooling under `test/diff/`; wasm89's interpreter
already evaluates `try_table` and is not modified.*

## MUST

* The harness MUST run a `try_table` catch-and-return template fixture
  (`fixtures/ct.wasm`) under wasm89, wasmtime, and wasm-interp.
* A throwing-and-caught call MUST report PASS with the thrown payload as
  the returned value across all three engines.
* A non-throwing call to the same function MUST report PASS with the
  fallthrough result across all three engines.
* wasm-interp MUST run the fixture with `--enable-exceptions`; wasmtime
  runs it without an extra flag.
* The full sweep MUST keep zero mismatches and zero errors with the
  fixture included.

## MUST NOT

* The harness MUST NOT report PASS when the engines disagree on the caught
  value or on the normal-path value.
* The harness MUST NOT require wasm89 to be modified for this coverage.
