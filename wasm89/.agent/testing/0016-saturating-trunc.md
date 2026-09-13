# Testing: saturating float-to-integer conversion

*BDD scenarios for implementing the eight `*_trunc_sat_*` instructions in
the evaluator. These are the `0xFC` sub-opcodes `0x00`-`0x07`
(`validate.c:3258` uses `w89_tsat_tab`); decode (`decoder.c`) and validate
already accept them, and the primitives exist in `numeric.c`, but
`step_bulk` (`eval.c`) does not dispatch them and falls through to a
crash.*

## SAT-001 f32 -> i32 signed/unsigned

SCENARIO: Saturated +inf
GIVEN `f32.const +inf` then `i32.trunc_sat_f32_s`
WHEN evaluated
THEN the result is `0x7fffffff` (no trap).
GIVEN `f32.const +inf` then `i32.trunc_sat_f32_u`
THEN the result is `0xffffffff`.

SCENARIO: Saturated -inf
GIVEN `f32.const -inf` then `i32.trunc_sat_f32_s`
WHEN evaluated
THEN the result is `0x80000000`.

SCENARIO: NaN truncates to zero
GIVEN `f32.const` any NaN then `i32.trunc_sat_f32_s` (or `_u`)
WHEN evaluated
THEN the result is `0` (no trap).

SCENARIO: Fractional truncates toward zero
GIVEN `f32.const -0.5` then `i32.trunc_sat_f32_s`
WHEN evaluated
THEN the result is `0`.

## SAT-002 f64 -> i32 signed/unsigned

SCENARIO: Out-of-range saturates
GIVEN `f64.const 1e300` then `i32.trunc_sat_f64_s`
WHEN evaluated
THEN the result is `0x7fffffff`.
GIVEN `f64.const -1e300` then `i32.trunc_sat_f64_s`
THEN the result is `0x80000000`.

## SAT-003 f32/f64 -> i64 signed/unsigned

SCENARIO: i64 bounds
GIVEN `f32.const +inf` then `i64.trunc_sat_f32_s`
WHEN evaluated
THEN the result is `0x7fffffffffffffff`.
GIVEN `f64.const -inf` then `i64.trunc_sat_f64_s`
THEN the result is `0x8000000000000000`.
GIVEN `f64.const -1.0` then `i64.trunc_sat_f64_u`
THEN the result is `0`.

## SAT-004 Never traps

SCENARIO: All saturating conversions trap-free
GIVEN any NaN, infinite, or out-of-range float operand to any of the eight
`*_trunc_sat_*` instructions
WHEN evaluated
THEN evaluation completes with the saturated result and does not raise
    "unsupported bulk instruction in evaluator" nor a trap.
