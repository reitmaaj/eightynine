# Acceptance: memory64 and multi-memory differential

*Acceptance criteria for memory64 and multi-memory differential coverage
(scenarios `0019`). Test tooling under `test/diff/`; wasm89 already
evaluates memory64 and multi-memory and is not modified.*

## MUST

* The harness MUST run a `memory i64` template fixture (`mem64.wasm`)
  under wasm89, wasmtime, and wasm-interp and require equal results.
* The harness MUST run a two-memory template fixture (`mm.wasm`) under all
  three engines, covering both a `(memory 1)` access and the default
  memory-0 path, and require equal results.
* wasm-interp MUST be invoked with `--enable-memory64` /
  `--enable-multi-memory` and wasmtime with `-W memory64` /
  `-W multi-memory` for the respective fixtures.
* The full sweep MUST keep zero mismatches and zero errors with these
  fixtures included.

## MUST NOT

* The harness MUST NOT report PASS when engines disagree on a memory64 or
  second-memory result.
* The harness MUST NOT require wasm89 to be modified for this coverage.
