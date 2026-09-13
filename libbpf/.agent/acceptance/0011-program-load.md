# 0011 - Acceptance: program loading and immutable validation

*Acceptance criteria for the sandbox Phase 1 loader. Each item is either a
behaviour the software MUST exhibit or a behaviour it MUST reject. A rejected
program MUST yield no executable program object.*

## MUST

* `bpf_program_load` MUST accept a buffer of whole, valid, in-profile eBPF
  bytecode and produce an opaque, immutable `bpf_program` reporting its decoded
  instruction count.
* Every branch and program-local CALL offset MUST be resolved at load time to an
  instruction index measured in 64-bit slots, counting the second slot of wide
  instructions (RFC 9669 section 4.3).
* Reading r10 as a base register for a memory operation MUST be permitted.
* A program whose required conformance groups are all allowed by the profile
  MUST load.
* The headers MUST fail to compile unless the underlying host types have the
  exact required widths (64-bit `unsigned long`/`long`, 32-bit
  `unsigned int`/`int`, 16-bit `unsigned short`).

## MUST NOT

* The loader MUST NOT accept a buffer whose length is not a whole number of
  8-byte slots, an empty buffer, or a buffer truncated inside a wide
  instruction.
* The loader MUST NOT accept an unknown/reserved ALU, ALU64, JMP, or JMP32
  operation code.
* The loader MUST NOT accept a wide instruction whose reserved continuation
  fields are non-zero.
* The loader MUST NOT accept a deprecated packet (ABS/IND) instruction.
* The loader MUST NOT accept a branch or local-CALL target that lands inside the
  interior slot of a wide instruction or past the end of the program.
* The loader MUST NOT accept any explicit write to r10, nor an implicit atomic
  fetch write to r10.
* The loader MUST NOT accept a program whose conformance requirements exceed
  the allowed profile.
* A rejected load MUST NOT return a usable `bpf_program` and MUST report a
  specific error code.
