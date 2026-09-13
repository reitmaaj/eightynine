# Acceptance: exceptions differential

*Acceptance criteria for cross-engine exception differential coverage
(scenarios `0017`). Test tooling under `test/diff/`; the wasm89 interpreter
already evaluates exceptions and is not modified.*

## MUST

* The harness MUST run an exception template fixture under wasm89,
  wasmtime, and wasm-interp and compare outcomes.
* An unconditional-throw exported function MUST be reported PASS when all
  three engines terminate abnormally.
* A conditional-throw function MUST be reported PASS returning the value
  `0` when its argument is `0` (all three ok), and PASS (all three
  abnormal) for a throwing argument.
* wasm89's `@exception` outcome MUST be treated as a runtime failure for
  comparison, so it agrees with an engine that also terminates abnormally
  and disagrees (FAIL/ERROR) with an engine that returns ok.
* wasmtime MUST classify an uncaught exception (e.g. "thrown Wasm
  exception") as a runtime failure, not as an engine error.
* wasm-interp MUST run the exception fixture with `--enable-exceptions`;
  wasmtime MUST run it without an extra flag (exceptions enabled by
  default).
* The sweep MUST keep zero mismatches and zero errors on the whole corpus
  including the exception fixture.

## MUST NOT

* The harness MUST NOT report PASS when wasm89 terminates abnormally but an
  engine returns ok.
* The harness MUST NOT treat an uncaught wasmtime exception as an engine
  ERROR (that would mask an abnormal-termination agreement).
