# 0002 - Testing: validator

*BDD scenarios for the validator milestone (RFC 9669 sections 3-5). Related
items are bundled.*

## 0002-001 Register range

SCENARIO: Valid registers are accepted
GIVEN an instruction using registers in 0..10
WHEN bpf_validate runs
THEN it reports no register error.

SCENARIO: Out-of-range registers are rejected (unacceptable behaviour)
GIVEN an instruction whose src_reg or dst_reg is greater than 10
WHEN bpf_validate runs
THEN it reports a register-range error.

## 0002-002 Deprecated packet access

SCENARIO: Legacy packet access is rejected (unacceptable behaviour)
GIVEN an LD instruction with ABS or IND mode
WHEN bpf_validate runs
THEN it reports a deprecated-packet error
AND the required conformance groups include the packet group.

## 0002-003 Host-specific 64-bit immediate subtypes

SCENARIO: Supported LD IMM subtype is accepted
GIVEN an LD IMM DW instruction with src_reg subtype 0
WHEN bpf_validate runs
THEN it reports no error.

SCENARIO: Host-specific LD IMM subtypes are rejected (unacceptable behaviour)
GIVEN an LD IMM DW instruction with src_reg subtype 1..6 (map_fd, map_val,
      var_addr, code_addr, map_idx, map_val_idx)
WHEN bpf_validate runs
THEN it reports an unsupported-immediate error.

## 0002-004 Byte swap width

SCENARIO: Valid END widths are accepted
GIVEN END instructions with imm width 16, 32, or 64
WHEN bpf_validate runs
THEN it reports no error.

SCENARIO: Invalid END width is rejected (unacceptable behaviour)
GIVEN an END instruction whose imm is not 16, 32, or 64
WHEN bpf_validate runs
THEN it reports a byte-swap-width error.

SCENARIO: ALU64 END source must be zero (unacceptable behaviour)
GIVEN an ALU64 END instruction whose source bit is set
WHEN bpf_validate runs
THEN it reports an error.

## 0002-005 NEG and MOVSX source

SCENARIO: NEG with K source is accepted
GIVEN a NEG instruction with source K
WHEN bpf_validate runs
THEN it reports no error.

SCENARIO: NEG with X source is rejected (unacceptable behaviour)
GIVEN a NEG instruction with source X
WHEN bpf_validate runs
THEN it reports an error.

SCENARIO: MOVSX requires register source (unacceptable behaviour)
GIVEN a MOVSX instruction with source K
WHEN bpf_validate runs
THEN it reports an error.

## 0002-006 CALL and EXIT forms

SCENARIO: CALL with src_reg 0, 1, or 2 is accepted
GIVEN CALL instructions with src_reg 0 (static helper), 1 (local), or 2 (BTF)
WHEN bpf_validate runs
THEN it reports no error.

SCENARIO: EXIT form is strict (unacceptable behaviour)
GIVEN an EXIT instruction with nonzero src_reg, offset, or imm
WHEN bpf_validate runs
THEN it reports an error.

## 0002-007 Conformance group tagging

SCENARIO: base32 group is always required
GIVEN any valid program
WHEN bpf_validate runs
THEN the base32 group is set in the required conformance bits.

SCENARIO: ALU64 instructions require base64
GIVEN an ALU64 instruction
WHEN bpf_validate runs
THEN the base64 group is set in the required conformance bits.

SCENARIO: DW memory access requires base64
GIVEN a load/store with DW size
WHEN bpf_validate runs
THEN the base64 group is set in the required conformance bits.

SCENARIO: ALU divmod requires divmul32
GIVEN a 32-bit ALU DIV or MOD instruction
WHEN bpf_validate runs
THEN the divmul32 group is set in the required conformance bits.

SCENARIO: ALU64 divmod requires divmul64
GIVEN a 64-bit ALU64 DIV or MOD instruction
WHEN bpf_validate runs
THEN the divmul64 group is set in the required conformance bits.

SCENARIO: ATOMIC W requires atomic32
GIVEN an ATOMIC mode W store
WHEN bpf_validate runs
THEN the atomic32 group is set in the required conformance bits.

SCENARIO: ATOMIC DW requires atomic64
GIVEN an ATOMIC mode DW store
WHEN bpf_validate runs
THEN the atomic64 group is set in the required conformance bits.
