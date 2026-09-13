# 0009 - Testing: arithmetic edge cases

*BDD scenarios for exhaustive ALU/ALU64, division/modulo, byte-swap, MOVSX,
and shift coverage. Related items are bundled.*

## 0009-001 ALU64 arithmetic extremes

SCENARIO: 64-bit ADD/SUB/MUL/OR/AND/XOR wrap correctly
GIVEN dst and src at extremes (0, 1, 2^63, max)
WHEN each ALU64 op executes
THEN the result wraps mod 2^64.

## 0009-002 ALU32 zero-extension

SCENARIO: 32-bit ops zero the upper bits for every operator
GIVEN a dst with high bits set and each ALU op
WHEN it executes
THEN the result occupies the low 32 bits.

## 0009-003 Division by zero

SCENARIO: Unsigned and signed division by zero yield 0
GIVEN a divisor of zero for DIV and SDIV at both widths
WHEN it executes
THEN the destination is 0.

## 0009-004 Modulo by zero

SCENARIO: 64-bit modulo by zero leaves the destination unchanged
GIVEN a divisor of zero for MOD and SMOD (64-bit)
WHEN it executes
THEN the destination is unchanged.

## 0009-005 Signed division overflow

SCENARIO: INT_MIN / -1 does not trigger undefined behaviour
GIVEN a dividend of INT_MIN (or INT64_MIN) and divisor -1
WHEN SDIV executes
THEN the result is defined (INT_MIN) without UB.

## 0009-006 Signed modulo truncates

SCENARIO: Signed modulo uses truncated division
GIVEN negative dividends and divisors
WHEN SMOD executes
THEN the result matches C's truncated modulo (e.g. -13 % 3 == -1).

## 0009-007 Shift masks

SCENARIO: Shift counts are masked per width
GIVEN counts 0, 31, 32, 63, 64, 70, and 0x3F/0x1F masks
WHEN LSH/RSH/ARSH execute at both widths
THEN the effective count is count & mask.

## 0009-008 Arithmetic shift right

SCENARIO: ARSH sign-extends
GIVEN a negative destination and ARSH
WHEN it executes
THEN the high bits are filled with the sign bit.

## 0009-009 Byte swap

SCENARIO: END swaps 16/32/64 bits
GIVEN byte patterns and each END width
WHEN END executes (LE/BE for ALU, unconditional for ALU64)
THEN the bytes within the width are reversed.

## 0009-010 MOVSX

SCENARIO: MOVSX sign-extends 8/16/32-bit operands
GIVEN operands whose sign bit is set
WHEN MOVSX executes at both widths
THEN the destination is sign-extended.

## 0009-011 MOV K sign-extension

SCENARIO: MOV K sign-extends the immediate into 64 bits
GIVEN imm values with the high bit set
WHEN ALU64 MOV K executes
THEN the destination is the sign-extended value.
