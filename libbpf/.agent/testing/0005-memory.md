# 0005 - Testing: memory operations

*BDD scenarios for the memory milestone (RFC 9669 sections 5.1, 5.2, 5.4).
Related items are bundled.*

## 0005-001 64-bit immediate load

SCENARIO: LD IMM DW loads a 64-bit immediate
GIVEN an LD IMM DW instruction with subtype 0
WHEN it executes
THEN the destination holds `(next_imm << 32) | imm`.

## 0005-002 Regular load (LDX MEM)

SCENARIO: LDX loads an unsigned value from memory
GIVEN a memory region with known bytes
WHEN an LDX MEM B/H/W/DW executes at a valid address
THEN the destination holds the zero-extended little-endian value.

## 0005-003 Regular store

SCENARIO: ST and STX write to memory
GIVEN a destination address in bounds
WHEN an ST (imm) or STX (register) MEM instruction executes
THEN the bytes at the address hold the little-endian value of the given size.

## 0005-004 Sign-extension load

SCENARIO: LDX MEMSX sign-extends a loaded value
GIVEN a memory byte 0x80
WHEN an LDX MEMSX B executes
THEN the destination is sign-extended to -128.

## 0005-005 Out-of-bounds access is rejected

SCENARIO: A load or store beyond the memory region traps (unacceptable
          behaviour)
GIVEN a memory region of size N
WHEN a load/store addresses beyond the region (or wraps)
THEN the machine reports TRAP
AND no out-of-bounds memory is read or written.

## 0005-006 Negative address is rejected

SCENARIO: A negative effective address traps (unacceptable behaviour)
GIVEN a base register such that base + offset is negative
WHEN a load/store executes
THEN the machine reports TRAP.

## 0005-007 Address arithmetic

SCENARIO: The effective address is base register plus the signed offset
GIVEN a base register and an offset
WHEN a load executes
THEN it accesses `base + offset`.
