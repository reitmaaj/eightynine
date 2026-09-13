# 0011 - Testing: program loading and immutable validation

*BDD scenarios for the sandbox Phase 1 loader (the only route into execution).
A validated `bpf_program` is immutable: strict encodings, resolved branch and
call targets, the frame-pointer contract, exact-width arithmetic, and the
execution profile are all settled at load time.*

## 0011-001 Loader is the only route into execution

SCENARIO: A validated program is created from raw bytecode
GIVEN a raw buffer of valid eBPF bytecode and an allowed profile
WHEN the loader `bpf_program_load` is called
THEN it returns BPF_OK and an opaque `bpf_program`
AND the program reports the decoded instruction count.

## 0011-002 Input size and allocation arithmetic

SCENARIO: Input size must be whole instructions (unacceptable behaviour)
GIVEN a raw buffer whose length is not a multiple of 8 bytes
WHEN `bpf_program_load` is called
THEN it is rejected with a truncation error and returns no program.

SCENARIO: A truncated wide instruction is rejected (unacceptable behaviour)
GIVEN a buffer that ends inside the second 8-byte slot of a wide instruction
WHEN `bpf_program_load` is called
THEN it is rejected and returns no program.

SCENARIO: A program with no instructions is rejected (unacceptable behaviour)
GIVEN an empty raw buffer
WHEN `bpf_program_load` is called
THEN it is rejected and returns no program.

## 0011-003 Strict encodings and reserved fields

SCENARIO: Unknown ALU operations are rejected (unacceptable behaviour)
GIVEN an ALU/ALU64 instruction whose operation code is reserved (0xE or 0xF)
WHEN `bpf_program_load` is called
THEN it is rejected with an unsupported-operation error.

SCENARIO: Unknown JMP operations are rejected (unacceptable behaviour)
GIVEN a JMP/JMP32 instruction whose operation code is reserved (0xE or 0xF)
WHEN `bpf_program_load` is called
THEN it is rejected with an unsupported-operation error.

SCENARIO: Reserved continuation bytes of a wide instruction are rejected
       (unacceptable behaviour)
GIVEN a wide (IMM LD) instruction whose second-slot opcode/registers fields are
      not the reserved form
WHEN `bpf_program_load` is called
THEN it is rejected.

SCENARIO: Deprecated packet access is rejected (unacceptable behaviour)
GIVEN a legacy ABS/IND packet access instruction
WHEN `bpf_program_load` is called
THEN it is rejected as deprecated.

## 0011-004 Branch and call target resolution

SCENARIO: Branch offsets are measured in 64-bit slots including the second
          slot of a wide instruction
GIVEN a jump whose offset, counted in slots, crosses a wide instruction
WHEN `bpf_program_load` resolves the target
THEN the resolved taken-target instruction index equals the slot-correct index
AND differs from a naive compressed-index computation.

SCENARIO: A branch landing inside a wide continuation is rejected (unacceptable
          behaviour)
GIVEN a branch offset that targets the interior second slot of a wide
      instruction
WHEN `bpf_program_load` is called
THEN it is rejected with a branch-target error.

SCENARIO: A branch past the end of the program is rejected (unacceptable
          behaviour)
GIVEN a branch offset that targets beyond the final instruction
WHEN `bpf_program_load` is called
THEN it is rejected with a branch-target error.

SCENARIO: A backward branch is allowed subject to the budget
GIVEN a valid backward branch within the program
WHEN `bpf_program_load` is called
THEN it is accepted.

## 0011-005 Frame-pointer contract

SCENARIO: A program may read r10 as a base (unacceptable to forbid)
GIVEN a memory instruction that uses r10 only as a base register
WHEN `bpf_program_load` is called
THEN it is accepted.

SCENARIO: An explicit write to r10 is rejected (unacceptable behaviour)
GIVEN an ALU/ALU64 instruction writing r10 as its destination
WHEN `bpf_program_load` is called
THEN it is rejected with a frame-pointer error.

SCENARIO: An implicit atomic fetch write to r10 is rejected (unacceptable
          behaviour)
GIVEN an atomic fetch operation whose source register is r10
WHEN `bpf_program_load` is called
THEN it is rejected with a frame-pointer error.

## 0011-006 Execution profile

SCENARIO: A program outside the allowed conformance profile is rejected
       (unacceptable behaviour)
GIVEN a program requiring a conformance group that the profile forbids
WHEN `bpf_program_load` is called
THEN it is rejected with a profile error.

## 0011-007 Exact-width integer contract

SCENARIO: Exact widths are enforced at compilation
GIVEN a compilation of the headers
THEN it fails unless unsigned long and long are exactly 64 bits, unsigned int
     and int exactly 32 bits, and unsigned short exactly 16 bits.
