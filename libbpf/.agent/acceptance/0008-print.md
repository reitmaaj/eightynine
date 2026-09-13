# 0008 - Acceptance: print module

*Acceptance criteria for the print/format module. Each item is either a
behaviour the software MUST exhibit or a behaviour it MUST reject.*

## MUST

* `bpf_err_name` MUST return a distinct, nonempty string for every `bpf_err`
  value, and never NULL.
* `bpf_regs_byte` MUST return `(src << 4) | (dst & 0x0F)`.
* `bpf_dis_one` MUST write a nonempty, mnemonic-bearing string for every
  instruction class: ALU/ALU64 ops carry their name and a `32`/`64` suffix,
  JMP/JMP32 carry their condition name, and load/store carry their mode and
  size.
* `bpf_groups_into` MUST write the required conformance group names
  newline-separated, with `base32` always first.
* The CLI MUST produce the same output for `version`, `run`, `dis`, and
  `groups` after the refactor as before it.

## MUST NOT

* A print helper MUST NOT read past the instruction or buffer bounds it is
  given.
* `bpf_dis_one` MUST NOT write past the caller's output buffer.
