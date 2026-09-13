# 0005 - Acceptance: memory operations

*Acceptance criteria for the memory milestone (RFC 9669 sections 5.1, 5.2,
5.4). Each item is either a behaviour the software MUST exhibit or a behaviour
it MUST reject.*

## MUST

* LD IMM DW (subtype 0) MUST load `(next_imm << 32) | imm` into the
  destination register.
* LDX MEM MUST zero-extend the loaded B/H/W/DW value; MEMSX MUST sign-extend
  the loaded B/H/W value.
* ST MUST store the sign-extended immediate, and STX the source register, as a
  little-endian value of the instruction's size.
* Every load/store MUST access the effective address `base + offset`.

## MUST NOT

* A load/store whose effective address falls outside `[0, mem_size)` for the
  access width MUST NOT read or write memory; it MUST report TRAP.
* An effective address that overflows 64 bits or is negative MUST be reported
  as TRAP.
* The interpreter MUST NOT read or write beyond the supplied memory region on
  any path.
