# 0001 - Testing: decoder

*BDD scenarios for the decoder milestone (RFC 9669 section 3). Related items
are bundled.*

## 0001-001 Basic instruction decodes fields

SCENARIO: A basic 64-bit instruction is decoded into its fields
GIVEN a buffer containing the 8 bytes for a MOV64 r1, imm instruction
      (opcode 0xb7, regs byte 0x01, offset 0x0000, imm 0x11223344)
WHEN bpf_decode runs over the buffer
THEN it succeeds with one instruction
AND the instruction class is ALU64, code is MOV, source is K
AND dst_reg is 1 and src_reg is 0
AND offset is 0 and imm is 0x11223344
AND is_wide is false.

## 0001-002 Offset is sign-extended

SCENARIO: The 16-bit offset is sign-extended
GIVEN a basic instruction whose offset bytes are 0xFF 0xFF
WHEN bpf_decode runs
THEN the decoded offset is -1.

## 0001-003 Immediate is sign-extended

SCENARIO: The 32-bit immediate is sign-extended
GIVEN a basic instruction whose imm bytes are 0xFF 0xFF 0xFF 0xFF
WHEN bpf_decode runs
THEN the decoded imm is -1.

## 0001-004 Wide instruction decodes both immediates

SCENARIO: A wide 128-bit instruction is decoded
GIVEN a buffer containing 16 bytes for an LD IMM DW (opcode 0x18)
      with imm 0x11111111 and next_imm 0x22222222
WHEN bpf_decode runs
THEN it succeeds with one instruction
AND is_wide is true
AND imm is 0x11111111 and next_imm is 0x22222222
AND dst_reg is the requested register and src_reg is the subtype.

## 0001-005 Truncated wide instruction is rejected

SCENARIO: A wide instruction with insufficient bytes is rejected (unacceptable
          behaviour)
GIVEN a buffer where a wide LD IMM instruction (opcode 0x18) is the last
      element and fewer than 16 bytes remain
WHEN bpf_decode runs
THEN it reports a truncated-instruction error
AND it does not consume past the available bytes.

## 0001-006 Multiple instructions decode in order

SCENARIO: A sequence of instructions decodes sequentially
GIVEN a buffer containing two 8-byte basic instructions
WHEN bpf_decode runs
THEN it decodes two instructions in order with the correct per-instruction
      fields.

## 0001-007 Register field decomposition

SCENARIO: The regs byte splits into src_reg and dst_reg
GIVEN a basic instruction whose regs byte is 0x12
WHEN bpf_decode runs
THEN src_reg is 1 and dst_reg is 2.

## 0001-008 Class decomposition for each opcode class

SCENARIO: The low three opcode bits select the class
GIVEN opcodes with class bits LD(0), LDX(1), ST(2), STX(3), ALU(4), JMP(5),
      JMP32(6), ALU64(7)
WHEN bpf_decode runs
THEN the decoded class matches the opcode's low three bits.

## 0001-009 Mode/size/code decomposition

SCENARIO: ALU/JMP opcodes split into code and source; LD/ST opcodes split into
          mode and size
GIVEN an ALU opcode 0x07 (ADD X ALU64) and an STX opcode 0x6A
      (mode MEM, size B, class STX)
WHEN bpf_decode runs
THEN the ALU instruction has code ADD, source X, class ALU64
AND the STX instruction has mode MEM, size B, class STX.
