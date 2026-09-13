# Testing: scalar integer operations

*BDD scenarios for the values milestone (spec 4.3.2 Integer Operations).
Bundled here are all i32/i64 scalar integer op scenarios.*

## INT-001 Addition, subtraction, multiplication

SCENARIO: Arithmetic wraps modulo 2^N
GIVEN two N-bit integer bit patterns a and b
WHEN iadd/isub/imul is applied
THEN the result is (a op b) mod 2^N
AND the operations never trap.
WHEN the result would exceed the N-bit range
THEN it wraps (e.g. 0xFFFFFFFF + 1 = 0 as i32).

## INT-002 Division

SCENARIO: Unsigned division truncates toward zero
GIVEN a non-zero divisor b
WHEN idiv_u is applied
THEN the result is a / b truncated toward zero.

SCENARIO: Division by zero traps
GIVEN a divisor bit pattern of 0
WHEN idiv_u or idiv_s is applied
THEN the operation traps and yields no result.

SCENARIO: Signed division overflow traps
GIVEN a = -2^(N-1) and b = -1 as N-bit signed values
WHEN idiv_s is applied
THEN the operation traps (the quotient +2^(N-1) is not representable).

## INT-003 Remainder

SCENARIO: Remainders follow the dividend sign
GIVEN a non-zero divisor b
WHEN irem_s is applied
THEN the result has the sign of the dividend.
WHEN irem_u is applied
THEN the result is the unsigned remainder.

SCENARIO: Remainder by zero traps
GIVEN a divisor of 0
WHEN irem_u or irem_s is applied
THEN the operation traps.

SCENARIO: Remainder of INT_MIN by -1 does not trap
GIVEN a = -2^(N-1), b = -1
WHEN irem_s is applied
THEN the result is 0 and the operation succeeds.

## INT-004 Bitwise and shifts

SCENARIO: Bitwise ops are exact
GIVEN bit patterns a and b
WHEN iand, ior, ixor are applied
THEN the result is the bitwise and/or/xor.

SCENARIO: Shift counts are taken modulo N
GIVEN shift count c
WHEN ishl/ishr_u/ishr_s is applied
THEN the effective count is c mod N.
WHEN the count is 0
THEN the value is unchanged.
WHEN ishr_s is applied to a negative value
THEN the vacated bits are filled with the sign bit.

## INT-005 Rotations

SCENARIO: Rotations shift bits around
GIVEN count c and value a
WHEN irotl is applied
THEN the result is a rotated left by c mod N.
WHEN irotr is applied
THEN the result is a rotated right by c mod N.
WHEN c mod N is 0
THEN the value is unchanged.

## INT-006 Count and test ops

SCENARIO: clz/ctz/popcnt
GIVEN a value
WHEN iclz is applied
THEN the result is the count of leading zero bits (N for zero).
WHEN ictz is applied
THEN the result is the count of trailing zero bits (N for zero).
WHEN ipopcnt is applied
THEN the result is the number of set bits.
WHEN ieqz is applied
THEN the result is 1 for zero, else 0.

## INT-007 Comparisons

SCENARIO: Comparisons
GIVEN two values a and b
WHEN ieq/ine is applied
THEN the result is 1 iff the bit patterns are equal/unequal.
WHEN a signed comparison (lt_s/gt_s/le_s/ge_s) is applied
THEN the comparison uses the signed interpretation.
WHEN an unsigned comparison (lt_u/gt_u/le_u/ge_u) is applied
THEN the comparison uses the unsigned interpretation.
WHEN a signed and unsigned comparison of the same bit patterns differ
    (e.g. 0xFFFFFFFF < 0) 
THEN each yields its own correct result.
