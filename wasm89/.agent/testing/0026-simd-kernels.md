# Testing: SIMD — numeric kernels (S3.1.B2)

*BDD scenarios for the SIMD per-lane numeric kernels (PLAN S3.1): integer
and float lane ops, comparisons, shuffles/swizzle, bitmask, dot/extmul, and
lane load/store. Stakeholder value: story 0000. Design:
`.agent/design/0010-simd.md`.*

## SIM-004 Integer lane arithmetic is exact per lane

SCENARIO: Wrapping integer lane ops
GIVEN `i8x16`/`i16x8`/`i32x4`/`i64x2` add/sub/mul/… on two vectors
WHEN evaluated
THEN each lane is computed independently with wrapping integer semantics,
    matching the scalar op of that lane width, with no cross-lane carry.

SCENARIO: Saturating and narrow/wide lane ops
GIVEN the SIMD integer ops that saturate, narrow, or widen lanes
WHEN evaluated
THEN lane results match the reference for edge values (min/max/sign
    bounds) exactly.

## SIM-005 Float lane ops follow the deterministic NaN profile

SCENARIO: Float lane arithmetic
GIVEN `f32x4`/`f64x2` add/sub/mul/div/sqrt/etc. on two vectors
WHEN evaluated
THEN each lane is computed independently
AND NaN-producing lanes are canonicalized per the project's deterministic
    float profile (same rule as scalar f32/f64).

## SIM-006 Shuffles, swizzle, splats, extract/replace, bitmask

SCENARIO: Shuffle/swizzle honour index bounds
GIVEN a shuffle/swizzle with a lane index immediate (or per-lane operand)
WHEN an index is out of the legal range for a swizzle
THEN that lane yields 0 (swizzle semantics), while a shuffle with an
    out-of-range static immediate is rejected
AND otherwise each output lane equals the selected input lane.

SCENARIO: Splat/extract/replace are per-lane
GIVEN splat, extract_lane, and replace_lane
WHEN evaluated with a lane index
THEN splat replicates the scalar to all lanes; extract returns the indexed
    lane; replace overwrites exactly the indexed lane.

SCENARIO: Bitmask and reductions
GIVEN the bitmask and horizontal-reduction lane ops
WHEN evaluated
THEN the bitmask packs the top bits of the sign-comparison lanes in the
    specified lane order.

## SIM-007 Load/store lanes and the wider load shapes

SCENARIO: Lane-granular load/store (load8x8 … load/store lane)
GIVEN `v128.load*` variants with lane shapes and `load`/`store` lane
WHEN evaluated
THEN each lane's bytes are read/written at the computed per-lane addresses
    with the same effective-address trap rules as a scalar access.

SCENARIO: Dot and extmul
GIVEN `i32x4.dot_i16x8_s` and the `extmul` variants
WHEN evaluated
THEN results match the reference for signed/zero extension and
    saturating-dot edge inputs.
