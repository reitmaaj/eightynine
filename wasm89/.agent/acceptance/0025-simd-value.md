# Acceptance: SIMD — v128 value, const, load/store (S3.1.B1)

*Relates to `.agent/testing/0025-simd-value.md`. Stakeholder value: story
0000. Design: `.agent/design/0010-simd.md`.*

## MUST

* A v128 value MUST hold all 128 bits and MUST round-trip unchanged through
  construction, copy, and storage (locals, globals, operands).
* The v128 runtime payload MUST NOT alias or truncate a scalar value in the
  unified value stack; scalar numerics MUST continue to operate unchanged.
* A v128 operand MUST be distinguishable from scalar operands at
  validation; any scalar instruction fed a v128 MUST be rejected, and vice
  versa.
* `v128.const` MUST decode and evaluate to a v128 whose bit pattern equals
  its 16 immediate bytes exactly.
* Inconsistent lane-shape immediates MUST be rejected (decode/validate)
  rather than yielding partial results.
* `v128.load`/`v128.store` MUST read/write the effective address with the
  same effective-address and `oob` trap rules as a scalar access of
  equivalent width.

## MUST NOT

* Widening the value model MUST NOT change any scalar numeric, memory,
  control, or reference result (full scalar suite MUST stay green).
* A v128 MUST NOT be silently truncated, padded, or reinterpreted as a
  scalar when crossing a memory/local/global/operand boundary.
* An out-of-bounds `v128.load`/`v128.store` MUST NOT silently succeed or
  read/write partial out-of-range bytes.
* A `v128` value type MUST NOT be mis-decoded from the reserved byte range
  or confused with a reference type.

## Gate

* `just lint ob89 test` green (scalar suite unaffected).
* New unit tests for the wide value, `v128.const`, and `v128.load`/store
  round-trip and trap cases pass; scalar `_simd` conformance subset that
  exercises const/load/store green.
