# Acceptance: cross-engine differential harness

*Acceptance criteria for the differential tester (design `0008`,
scenarios `0015`). Test tooling under `test/diff/`; must not alter the
interpreter's semantics or C89 build.*

## MUST

* The harness MUST be able to run an identical numeric case under wasm89,
  wasmtime, and wasm-interp and to reach a single normalized verdict for
  that case.
* Integer results (`i32`/`i64`) MUST be compared by raw bit equality.
* Finite `f32`/`f64` results MUST be compared by raw bit equality.
* A NaN result on one side MUST be equal to a NaN result on the other
  regardless of payload or sign; a NaN versus a finite result MUST be a
  FAIL.
* A status where wasm89 and the oracle both trap (or both exhaust) MUST be
  a PASS; trap messages MUST NOT be compared across engines.
* A status disagreement (ok on one side, trap/exhaustion on the other) MUST
  be a FAIL.
* A missing configured engine MUST be reported as an ERROR with a nonzero
  exit, never as a silent PASS.
* The generator MUST emit only milestone-1 programs (random type-correct
  scalar-numeric expressions plus fixed memory/control/call/trap template
  modules), MUST produce modules with no imports and no `start`, and MUST
  generate its corpus and argument vectors deterministically from a single
  integer seed.
* The sweep MUST bound every case and every module by a deadline, print
  per-module progress, tear down and restart a wedged runtime process, and
  account for every case as PASS, FAIL, or error with final totals.
* The harness self-test MUST include a case in which an oracle result is
  forced to disagree with wasm89, and the harness MUST report FAIL for it
  (proving the comparator can fail).
* wasmtime's finite results MUST be decoded from its shortest-round-trip
  decimal output back to exact bits, using the declared result width so an
  f32 is decoded as an f32.
* wasm-interp MUST be used as an oracle only when a case's parameter and
  result types are all integer (its float output is not bit-exact);
  float-typed cases MUST be compared against wasmtime.
* Engine discovery MUST be PATH-based; each engine MUST be invoked with
  the feature-flag set explicitly matching the module's used subset.

## MUST NOT

* The harness MUST NOT report PASS when finite results differ in any bit,
  when a NaN is compared to a finite value, or when result widths or
  counts differ.
* The harness MUST NOT treat an engine that is missing or whose output it
  cannot parse as a pass or a skip; both MUST be reported as errors.
* The harness MUST NOT generate, or report a green run over, SIMD/v128,
  GC/i31, exceptions, memory64, multi-memory, imported/mutable globals,
  host imports, or `start` modules in the milestone-1 generator.
* The harness MUST NOT feed exotic NaN payloads as cross-engine float
  *inputs*; only canonical NaN is fed, while NaN *results* remain subject
  to the NaN rule.
* The harness MUST NOT add the full sweep to `run_tests.sh`; `main` MUST
  remain buildable and green without any reference engine installed.
* The milestone-1 work MUST NOT modify `src/*.c`, `src/*.h`, or the
  existing C unit tests' behaviour.
