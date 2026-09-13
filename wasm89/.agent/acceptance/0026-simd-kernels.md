# Acceptance: SIMD — numeric kernels (S3.1.B2)

*Relates to `.agent/testing/0026-simd-kernels.md`. Stakeholder value: story
0000. Design: `.agent/design/0010-simd.md`.*

## MUST

* Integer lane ops (i8x16/i16x8/i32x4/i64x2) MUST compute each lane
  independently with wrapping integer semantics matching the scalar op of
  that width, with no cross-lane carry/borrow.
* Saturating/narrowing/widening lane ops MUST match the reference at edge
  values (min/max/sign bounds) exactly.
* Float lane ops (f32x4/f64x2) MUST compute each lane independently and
  MUST canonicalize NaN lanes per the project's deterministic float profile.
* Swizzle MUST yield 0 for an out-of-range runtime lane index; a shuffle
  with an out-of-range static immediate MUST be rejected.
* Splat MUST replicate to all lanes; extract/replace MUST address exactly
  the indexed lane.
* The bitmask op MUST pack lanes in the specified order.
* Lane-granular load/store MUST read/write the correct per-lane addresses
  with the same effective-address trap rules as scalar access.
* Dot/extmul MUST match the reference for signed/zero-extension and
  edge-input cases.

## MUST NOT

* A SIMD kernel MUST NOT read or write across a lane boundary or reuse a
  lane's data in another lane (except where the op definition requires it).
* Float lane results MUST NOT be left with an engine-dependent NaN payload;
  they MUST follow the deterministic profile.
* A lane-shape mismatch (wrong lane width/count for the opcode) MUST be
  rejected, not silently coerced.
* Scalar numeric behaviour MUST be unchanged; the scalar `just test` suite
  MUST stay green.

## Gate

* Per-op unit tests (each pure kernel + every non-trivial branch) pass.
* The SIMD numeric conformance subset (arith/compare/shuffle/bitmask/dot/
  extmul/lane-load-store .wast) green before any broader SIMD sweep.
