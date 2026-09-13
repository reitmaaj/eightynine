# Testing: floating-point operations

*BDD scenarios for the values milestone (spec 4.3.3 Floating-Point
Operations), under the deterministic profile (DET).*

## FLT-001 Rounding and arithmetic

SCENARIO: Arithmetic follows round-to-nearest ties-to-even
GIVEN two finite floating-point values
WHEN fadd/fsub/fmul/fdiv is applied
THEN the result is the exact mathematical result rounded to the target
    width with round-to-nearest-ties-even.
WHEN the exact result overflows the exponent range
THEN the result is infinity.
WHEN the exact result is 0/0, 0*inf, inf-inf, inf/inf, or produced from
    NaN inputs
THEN the result is the canonical positive NaN.

## FLT-002 Deterministic NaN

SCENARIO: All NaN results are canonical
GIVEN any operation whose result is a NaN, with any NaN inputs
WHEN the result is observed as a bit pattern
THEN it is exactly 0x7FC00000 for f32 and 0x7FF8000000000000 for f64
    (positive canonical NaN).

## FLT-003 fmin/fmax

SCENARIO: Minimum and maximum
GIVEN two non-NaN values
WHEN fmin is applied
THEN it returns the smaller, with -inf winning and +inf losing.
WHEN fmax is applied
THEN it returns the larger, with +inf winning and -inf losing.
GIVEN +0 and -0
WHEN fmin is applied
THEN it returns -0.
WHEN fmax is applied
THEN it returns +0.
GIVEN a NaN operand
WHEN fmin or fmax is applied
THEN the result is the canonical NaN.

## FLT-004 Sign operations preserve NaN payloads

SCENARIO: fneg/fabs/fcopysign do not canonicalize NaN
GIVEN a NaN with a non-canonical payload and a given sign
WHEN fneg, fabs, or fcopysign is applied
THEN the payload is preserved unchanged and only the sign is altered.

## FLT-005 Unary rounding ops

SCENARIO: sqrt/ceil/floor/trunc/nearest
GIVEN a finite value
WHEN fsqrt is applied
THEN the result is correctly rounded.
WHEN fceil is applied
THEN the result is the least integral value not less than the input.
WHEN ffloor is applied
THEN the result is the greatest integral value not greater than the
    input.
WHEN ftrunc is applied
THEN the result is the integral value of the same sign whose magnitude
    is not greater than the input.
WHEN fnearest is applied
THEN the result is the nearest integral value, ties to even, with
    0 < z <= 0.5 yielding +0 and -0.5 <= z < 0 yielding -0.
GIVEN infinity
WHEN fceil/ffloor/ftrunc/fnearest is applied
THEN the result is the same infinity.
GIVEN NaN
WHEN fsqrt/fceil/ffloor/ftrunc/fnearest is applied
THEN the result is the canonical NaN.

## FLT-006 Comparisons

SCENARIO: Comparisons
GIVEN two values
WHEN feq/fne/flt/fgt/fle/fge is applied
THEN the result is 1/0 per IEEE-754 comparison.
WHEN either operand is NaN
THEN feq/flt/fgt/fle/fge yield 0 and fne yields 1.
WHEN the operands are +0 and -0
THEN feq yields 1.
