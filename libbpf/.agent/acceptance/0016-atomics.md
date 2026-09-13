# 0016 - Acceptance: atomic read-modify-write over owned memory

*Acceptance criteria for atomic operations over owned guest memory. Each item
is a behaviour the software MUST exhibit or MUST reject.*

## MUST

* A non-fetch atomic (ADD/OR/AND/XOR) MUST combine the stored word with the
  register operand and store the result in place.
* A fetch atomic MUST store the combined value and return the prior stored
  value in the source register (or in r0 for CMPXCHG).
* XCHG MUST store the register operand and return the prior stored value.
* CMPXCHG MUST store the operand only when the prior value equals the expected
  value in r0, and MUST always report the prior value in r0.
* Word atomics MUST affect only the low 32 bits; double-word atomics the full
  64 bits.
* Every atomic MUST read and write through the region resolver over the exact
  byte width, so only exclusively owned, writable guest memory is touched.

## MUST NOT

* An atomic MUST NOT touch a read-only or unmapped address, or one that crosses
  a region boundary; such an access MUST trap without changing registers or
  memory.
* An atomic MUST NOT share state with a concurrent host; over exclusively owned
  guest memory the read-modify-write is performed as one machine step.
