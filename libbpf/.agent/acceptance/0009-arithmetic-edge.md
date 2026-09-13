# 0009 - Acceptance: arithmetic edge cases

*Acceptance criteria for exhaustive arithmetic coverage. Each item is either
a behaviour the software MUST exhibit or a behaviour it MUST NOT exhibit.*

## MUST

* 64-bit ALU64 ops MUST wrap mod 2^64 at extremes.
* 32-bit ALU ops MUST produce a result in the low 32 bits and zero the upper
  bits.
* Division by zero MUST yield 0 (both widths, signed and unsigned).
* 64-bit modulo by zero MUST leave the destination unchanged.
* Signed division/modulo MUST NOT trigger undefined behaviour for
  `INT_MIN / -1`; the result MUST be defined.
* Signed modulo MUST use truncated division.
* Shifts MUST mask the count by 0x3F (64-bit) or 0x1F (32-bit).
* ARSH MUST fill high bits with the sign bit.
* END MUST reverse the bytes within the selected width; LE is the identity,
  BE and ALU64 swap.
* MOVSX MUST sign-extend 8/16/32-bit operands; MOV K in ALU64 MUST sign-extend
  the immediate.

## MUST NOT

* Signed division/modulo MUST NOT invoke undefined behaviour (no `INT_MIN /
  -1`).
* A shift MUST NOT use an unmasked count.
* A 32-bit op MUST NOT leave stale high bits.
