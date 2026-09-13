# Testing: numeric conversions

*BDD scenarios for the values milestone (spec 4.3.4 Conversions and the
sign-extension instructions).*

## CONV-001 Integer wrap and extend

SCENARIO: wrap and extend
GIVEN an i64 bit pattern
WHEN i32.wrap_i64 is applied
THEN the result is the value modulo 2^32.
GIVEN an i32 bit pattern
WHEN i64.extend_i32_s is applied
THEN the result is the sign-extended value.
WHEN i64.extend_i32_u is applied
THEN the result is the zero-extended value.

## CONV-002 trunc to integer

SCENARIO: trunc truncates toward zero and traps on invalid input
GIVEN a finite float whose truncation fits the target
WHEN i32.trunc_f32_s etc. is applied
THEN the result is the truncation toward zero.
GIVEN a NaN, an infinity, or a value whose truncation is out of the
    target range
WHEN any trunc_* is applied
THEN it traps and produces no result.
GIVEN -0.5
WHEN i32.trunc_f32_u is applied
THEN the result is 0 (values in (-1, 0) are valid for unsigned truncation).

## CONV-003 Saturating truncation

SCENARIO: trunc_sat saturates instead of trapping
GIVEN a NaN
WHEN i32.trunc_sat_f32_s/u is applied
THEN the result is 0.
GIVEN +inf / -inf
WHEN the signed variant is applied
THEN the result is the maximum / minimum signed value.
WHEN the unsigned variant is applied
THEN the result is 0 (for -inf) or 2^N-1 (for +inf).
GIVEN a finite out-of-range value
WHEN the signed variant is applied
THEN the result saturates to the nearest representable value.

## CONV-004 Float conversions

SCENARIO: convert rounds float values
GIVEN an integer value
WHEN f32.convert_i64_u etc. is applied
THEN the result is the correctly rounded float (e.g.
    f32.convert_i32_u(0xFFFFFFFF) = 2^32 as f32).
SCENARIO: promote is exact and demote rounds
WHEN f64.promote_f32 is applied
THEN the result is exact.
WHEN f32.demote_f64 is applied
THEN the result is correctly rounded; a NaN result is canonical; an
    out-of-range magnitude becomes infinity.

## CONV-005 Reinterpret

SCENARIO: reinterpret preserves bit patterns
GIVEN an f32 bit pattern
WHEN i32.reinterpret_f32 is applied
THEN the identical bit pattern is returned as i32, and the reverse
    conversion round-trips.

## CONV-006 Sign extension of narrow integers

SCENARIO: extend8_s/extend16_s/extend32_s
GIVEN a value whose low bits are set at the sign position
WHEN i32.extend8_s etc. is applied
THEN the high bits are filled with that sign bit.
