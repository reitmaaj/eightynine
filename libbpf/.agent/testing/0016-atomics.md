# 0016 - Testing: atomic read-modify-write over owned memory

*BDD scenarios for atomic operations (ADD/OR/AND/XOR/XCHG/CMPXCHG, word and
double-word) executed over exclusively owned guest memory. Because the guest
memory is owned and not shared with the host, each atomic is an emulated
read-modify-write through the region resolver; a fetch operation writes the old
value back to a register, and CMPXCHG compares against r0.*

## 0016-001 Non-fetch atomics

SCENARIO: ADD/OR/AND/XOR update the word/double-word in place
GIVEN a stored word and a register operand
WHEN a non-fetch atomic operation is performed
THEN the stored word equals the prior value combined with the operand and no
     register is written with the old value.

## 0016-002 Fetch atomics

SCENARIO: A fetch atomic stores the new value and returns the old value
GIVEN a stored word and a fetch atomic operation
WHEN it is performed
THEN the source register receives the prior stored value and memory holds the
     combined value.

## 0016-003 XCHG

SCENARIO: XCHG swaps the stored word with the register
GIVEN a stored word and XCHG with a register operand
WHEN it is performed
THEN the register receives the prior word and memory holds the operand.

## 0016-004 CMPXCHG

SCENARIO: CMPXCHG stores only when the memory equals r0 and reports the old
          value in r0
GIVEN a stored word and an expected value in r0
WHEN CMPXCHG is performed
THEN r0 receives the prior value and memory is updated only when the prior
     value matched the expected value.

## 0016-005 Word truncation and memory errors

SCENARIO: Word atomics operate on the low 32 bits
GIVEN a word atomic on a double-word-sized register value
WHEN it is performed
THEN only the low word is affected.

SCENARIO: An atomic over an unmapped or read-only address traps (unacceptable
          behaviour)
GIVEN an atomic whose base resolves to an unmapped address or a read-only
     region
WHEN it is performed
THEN the machine traps and no register or memory is changed.
