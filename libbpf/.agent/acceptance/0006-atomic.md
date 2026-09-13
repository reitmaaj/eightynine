# 0006 - Acceptance: atomic operations

*Acceptance criteria for the atomic milestone (RFC 9669 section 5.3). Each
item is either a behaviour the software MUST exhibit or a behaviour it MUST
reject.*

## MUST

* ATOMIC ADD/OR/AND/XOR MUST update the addressed memory word (W or DW) with
  the operation's result, computed from the prior memory value and the source
  register.
* With the FETCH modifier, the source register MUST receive the prior memory
  value.
* XCHG MUST exchange the source register with the memory word.
* CMPXCHG MUST store the source register only when the memory word equals r0,
  and MUST load the prior memory value into r0 in both cases.
* 32-bit (W) atomics MUST operate on the low 32 bits and zero-extend.

## MUST NOT

* An ATOMIC operation MUST NOT modify memory when its address is out of
  bounds; it MUST report TRAP.
* An 8-bit or 16-bit ATOMIC operation MUST NOT be executed (rejected by the
  validator as ESIZE).
