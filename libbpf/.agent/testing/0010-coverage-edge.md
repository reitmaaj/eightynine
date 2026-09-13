# 0010 - Testing: coverage edge cases + benchmarks

*BDD scenarios for the final coverage edge cases (signed 32-bit division and
modulo, byte-swap width 8, 32-bit conditional-true branches, invalid-class
rejection, memory-size validation modes) and the benchmark harness. Related
items are bundled.*

## 0010-001 Signed 32-bit division edges

SCENARIO: op32_sdiv handles divisor 0 and a normal divisor
GIVEN a 32-bit signed division with a divisor of 0 and with a divisor that
is neither 0 nor -1
WHEN it executes
THEN divisor 0 yields 0 and the normal divisor yields the truncated quotient.

## 0010-002 Signed 32-bit modulo edges

SCENARIO: op32_smod handles divisor 0 and divisor -1
GIVEN a 32-bit signed modulo with a divisor of 0 and with a divisor of -1
WHEN it executes
THEN divisor 0 leaves the destination unchanged and divisor -1 yields 0.

## 0010-003 Byte-swap width 8

SCENARIO: END with width 8 is the identity
GIVEN an END instruction with width 8 in both ALU and ALU64
WHEN it executes
THEN the destination is unchanged (single byte has nothing to swap).

## 0010-004 32-bit conditional-true branches

SCENARIO: JMP32 JNE/JGE/JLT/JLE/JSGE/JSLE are taken when the condition holds
GIVEN a 32-bit conditional jump whose condition is true
WHEN it executes
THEN the jump is taken (pc advances by the offset).

## 0010-005 Invalid instruction class

SCENARIO: An out-of-range class byte is rejected at run time
GIVEN a decoded instruction whose class byte is not a recognised class
WHEN it is stepped
THEN the machine returns BPF_STAT_ERR.

## 0010-006 Memory-size validation modes

SCENARIO: validate_mem accepts MEM/MEMSX and rejects invalid sizes
GIVEN memory instructions with IMM-on-non-LD, MEM, MEMSX-B and MEMSX-DW modes
WHEN they are validated
THEN IMM-on-non-LD and MEMSX-DW are rejected with BPF_ESIZE and MEM/MEMSX-B
are accepted, and a JMP32 CALL with an invalid src is rejected with BPF_ECALL.

## 0010-007 Benchmark harness

SCENARIO: The benchmark runs representative programs and a micro matrix
GIVEN the bench harness with program-throughput and per-op cases
WHEN it runs
THEN every case executes a non-zero step count, returns, writes a
machine-readable CSV, and reports ns/step and MIPS.

## 0010-008 Unreachable defensive guards

SCENARIO: is_wide_ld's non-LD branch and mem_step's fallthrough are unreachable
GIVEN the public decoder and machine API
WHEN is_wide_ld is called only from the LD guard, and mem_step only from
handled LD/LDX/ST/STX classes
THEN the defensive return-0 / return-ERR branches are never executed and are
documented as unreachable, so they are excluded from line-coverage.
