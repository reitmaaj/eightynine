# 0010 - Acceptance: coverage edge cases + benchmarks

*Acceptance criteria for the coverage edge cases and the benchmark harness.
Each item is a behaviour the software MUST exhibit or MUST NOT exhibit.*

## MUST

* 32-bit signed division MUST yield 0 for a zero divisor and the truncated
  quotient for a normal divisor.
* 32-bit signed modulo MUST leave the destination unchanged for a zero
  divisor and yield 0 for a divisor of -1.
* END with width 8 MUST be the identity for both ALU and ALU64.
* JMP32 JNE/JGE/JLT/JLE/JSGE/JSLE MUST take the branch when the condition
  holds.
* An instruction with an out-of-range class byte MUST be rejected at run time
  with `BPF_STAT_ERR`.
* validate_mem MUST reject IMM-on-a-non-LD class and MEMSX-DW with
  `BPF_ESIZE`, and MUST accept MEM and MEMSX non-DW sizes.
* A JMP32 CALL with an invalid `src_reg` MUST be rejected with `BPF_ECALL`.
* The benchmark harness MUST run every case to `BPF_STAT_RETURNED` with a
  non-zero step count, write a machine-readable CSV, and report ns/step and
  MIPS.

## MUST NOT

* The benchmark harness MUST NOT report a case that fails to return or that
  executes zero steps as passing; it MUST fail instead.
* The benchmark MUST NOT crash on a malformed or non-returning program; it
  MUST report the failure.

## Unreachable-by-design (excluded from line coverage)

* `is_wide_ld`'s non-LD `return 0` (decode.c) is only reachable with class LD
  by construction of the caller; it is a defensive guard.
* `mem_step`'s terminal `return BPF_STAT_ERR` (eval.c) is only reachable with
  a class mem_step fully handles; it is a defensive guard.
