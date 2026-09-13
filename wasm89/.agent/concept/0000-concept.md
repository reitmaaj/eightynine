# Concept: wasm89 — a WebAssembly core runtime in pedantic C89

## Software

`wasm89` is a WebAssembly core runtime implemented in strict, portable C89.

## Primary goals, in order

1. **Strict compliance with the WebAssembly Core Specification**
   (W3C Candidate Recommendation Draft, 12 August 2026, release 3.0).
   Conformance is measured objectively against the official
   `WebAssembly/testsuite` mirror vendored under `vendor/testsuite-main`.
2. **Strict C89 conformance**: the codebase compiles cleanly under
   `-std=c89 -pedantic-errors -Wall -Wextra -Werror` with no warnings.
3. **Performance secondarily**: a straightforward switch-based interpreter
   with a typed operand stack; correctness and clarity outrank speed.

## Scope

* Target: full release 3.0 of the core spec, including multi-memory,
  memory64, exception handling, garbage collection, fixed-width SIMD and
  relaxed SIMD.
* Floating point follows the spec's **deterministic profile** (canonical
  positive NaN; fixed relaxed-SIMD behaviour) for reproducible results.
* Excluded by design (de-emphasized "extra features"):
  * text-format parsing (chapter 6) — tooling converts `.wast` to binary;
  * WASI and all non-core host APIs;
  * threading / shared memory (atomics are outside core 3.0);
  * the component model.
  These are rejected cleanly or simply not offered.

## Verifiability

Every feature milestone is gated on (a) a smoke test, (b) unit tests for
pure functions, (c) the relevant subset of the official testsuite, then
(d) the full testsuite. The build must stay warning-free at every commit.
