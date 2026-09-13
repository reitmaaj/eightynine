# 0002 - Acceptance: validator

*Acceptance criteria for the validator milestone. Each item is either a
behaviour the software MUST exhibit or a behaviour it MUST reject.*

## MUST

* `bpf_validate` validates every instruction in a decoded program.
* Registers MUST be in range 0..10; out-of-range registers MUST be reported.
* The required conformance group bits MUST be computed as the union across
  the program: every valid program requires `base32`; ALU64/JMP/DW-size and
  END-64 instructions add `base64`; 32-bit ALU div/mod add `divmul32`; 64-bit
  ALU div/mod add `divmul64`; ATOMIC W adds `atomic32`; ATOMIC DW adds
  `atomic64`.
* Byte-swap END instructions MUST have imm width 16, 32, or 64; other widths
  MUST be reported.

## MUST NOT

* `bpf_validate` MUST NOT accept the deprecated `packet` (LD ABS/IND)
  instructions; they MUST be reported and MUST set the packet conformance bit.
* `bpf_validate` MUST NOT accept host-specific LD IMM subtypes (src_reg 1..6);
  they MUST be reported as unsupported.
* `bpf_validate` MUST NOT accept a NEG instruction with source X, a MOVSX
  instruction with source K, or an ALU64 END instruction whose source bit is
  set.
* `bpf_validate` MUST NOT accept an EXIT instruction with nonzero src_reg,
  offset, or imm.
