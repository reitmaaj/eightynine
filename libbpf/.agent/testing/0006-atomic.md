# 0006 - Testing: atomic operations

*BDD scenarios for the atomic milestone (RFC 9669 section 5.3). Related items
are bundled.*

## 0006-001 Simple atomic operations

SCENARIO: Atomic ADD/OR/AND/XOR update memory
GIVEN a memory word and a source register
WHEN an ATOMIC ADD/OR/AND/XOR executes
THEN the memory word is updated with the operation result.

## 0006-002 Fetch modifier

SCENARIO: FETCH returns the prior value in the source register
GIVEN an ATOMIC operation with the FETCH modifier set
WHEN it executes
THEN the source register holds the memory value before the update.

## 0006-003 Exchange

SCENARIO: XCHG swaps the source register and memory
GIVEN a memory word and a source register
WHEN an ATOMIC XCHG executes
THEN memory holds the prior source value
AND the source register holds the prior memory value.

## 0006-004 Compare-and-exchange

SCENARIO: CMPXCHG writes only on a match and returns the old value in r0
GIVEN a memory word M and a source register S
WHEN an ATOMIC CMPXCHG executes with r0 equal to M
THEN memory becomes S
AND r0 holds the old value M.
WHEN an ATOMIC CMPXCHG executes with r0 not equal to M
THEN memory stays M
AND r0 holds M.

## 0006-005 32-bit atomic zero-extends

SCENARIO: 32-bit atomics operate on the low 32 bits
GIVEN a 64-bit source with high bits set
WHEN an ATOMIC W operation executes
THEN the result is the 32-bit operation, zero-extended.

## 0006-006 Bounds check

SCENARIO: An atomic out of bounds traps (unacceptable behaviour)
GIVEN an ATOMIC instruction whose address is out of bounds
WHEN it executes
THEN the machine reports TRAP
AND memory is not modified.
